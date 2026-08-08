#include <string.h>
#include "slp_session.h"

void
slp_session_init(struct slp_session *s, struct slp_transport transport)
{
    s->transport = transport;
    memset(&s->scanner, 0, sizeof(s->scanner));
    s->rxbuf_len = 0;
}

int
slp_session_poll(struct slp_session *s, int timeout_ms, slp_frame_cb cb
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
        int r = slp_frame_check(&s->scanner, s->rxbuf, (int)s->rxbuf_len);
        if (r == 0)
            break;

        size_t consume;
        if (r > 0) {
            struct slp_frame_view view;
            if (slp_frame_view_init(&view, s->rxbuf, r) == SLP_FRAME_OK) {
                if (cb)
                    cb(cb_ctx, view.msg_type, view.payload, view.payload_len);
                dispatched++;
            }
            // A version mismatch is silently dropped (not passed to cb)
            // per this module's own doc comment -- still consumed from
            // the buffer either way, since the frame itself was
            // otherwise well-formed (valid CRC/framing).
            consume = (size_t)r;
        } else {
            consume = (size_t)(-r);
        }

        memmove(s->rxbuf, s->rxbuf + consume, s->rxbuf_len - consume);
        s->rxbuf_len -= consume;
    }

    return dispatched;
}

bool
slp_session_send(struct slp_session *s, uint8_t msg_type
                 , const uint8_t *payload, size_t payload_len)
{
    if (payload_len > SLP_FRAME_MAX_PAYLOAD)
        return false;

    uint8_t frame[SLP_FRAME_MAX];
    size_t frame_len = slp_frame_encode(frame, msg_type, payload, payload_len);
    if (frame_len == 0)
        return false;

    int written = s->transport.write(s->transport.ctx, frame, frame_len);
    return written == (int)frame_len;
}
