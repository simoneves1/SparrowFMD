#include <string.h>
#include "khp_session.h"

void
khp_session_init(struct khp_session *s, struct khp_transport transport)
{
    s->transport = transport;
    memset(&s->scanner, 0, sizeof(s->scanner));
    s->rxbuf_len = 0;
    s->next_seq = 0;
}

int
khp_session_poll(struct khp_session *s, int timeout_ms, khp_message_cb cb
                 , void *cb_ctx)
{
    if (s->rxbuf_len < sizeof(s->rxbuf)) {
        int n = s->transport.read(s->transport.ctx, s->rxbuf + s->rxbuf_len
                                  , sizeof(s->rxbuf) - s->rxbuf_len
                                  , timeout_ms);
        if (n < 0)
            return n;
        s->rxbuf_len += (size_t)n;
    }

    int dispatched = 0;
    for (;;) {
        int r = khp_msgblock_check(&s->scanner, s->rxbuf, (int)s->rxbuf_len);
        if (r == 0)
            break; // not enough buffered data to decide either way yet

        size_t consume;
        if (r > 0) {
            struct khp_msgblock_view view;
            khp_msgblock_view_init(&view, s->rxbuf, r);
            if (cb)
                cb(cb_ctx, view.content, view.content_len, view.seq);
            dispatched++;
            consume = (size_t)r;
        } else {
            consume = (size_t)(-r); // garbage to discard, per
                                    // khp_msgblock_check's resync contract
        }

        memmove(s->rxbuf, s->rxbuf + consume, s->rxbuf_len - consume);
        s->rxbuf_len -= consume;
    }

    return dispatched;
}

bool
khp_session_send(struct khp_session *s, const uint8_t *content
                 , size_t content_len)
{
    if (content_len > KHP_MSG_MAX_CONTENT)
        return false;

    uint8_t block[KHP_MSG_MAX];
    size_t block_len = khp_msgblock_encode(block, content, content_len
                                           , s->next_seq);
    if (block_len == 0)
        return false;
    s->next_seq = (s->next_seq + 1) & KHP_MSG_SEQ_MASK;

    int written = s->transport.write(s->transport.ctx, block, block_len);
    return written == (int)block_len;
}
