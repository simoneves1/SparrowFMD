// Public declaration for kin_cartesian.c's stepper allocator. Klipper's
// own repo has no header for this -- klippy's Python side declares it via
// a cffi binding, not a C header, since chelper is normally built as a
// Python extension module, not linked directly into a C program the way
// it is here.
#ifndef KIN_CARTESIAN_H
#define KIN_CARTESIAN_H

struct stepper_kinematics;

// axis is 'x', 'y', or 'z'. Returns NULL for anything else.
struct stepper_kinematics *cartesian_stepper_alloc(char axis);

#endif // kin_cartesian.h
