#pragma once
// Don't import <linux/input.h> to avoid conflicting declarations
struct input_id {
    __u16 bustype;
    __u16 vendor;
    __u16 product;
    __u16 version;
};
struct input_absinfo {
    __s32 value;
    __s32 minimum;
    __s32 maximum;
    __s32 fuzz;
    __s32 flat;
    __s32 resolution;
};
struct input_keymap_entry {
#define INPUT_KEYMAP_BY_INDEX	(1 << 0)
    __u8  flags;
    __u8  len;
    __u16 index;
    __u32 keycode;
    __u8  scancode[32];
};
struct ff_replay {
    __u16 length;
    __u16 delay;
};
struct ff_trigger {
    __u16 button;
    __u16 interval;
};
struct ff_envelope {
    __u16 attack_length;
    __u16 attack_level;
    __u16 fade_length;
    __u16 fade_level;
};
struct ff_constant_effect {
    __s16 level;
    struct ff_envelope envelope;
};
struct ff_ramp_effect {
    __s16 start_level;
    __s16 end_level;
    struct ff_envelope envelope;
};
struct ff_condition_effect {
    __u16 right_saturation;
    __u16 left_saturation;

    __s16 right_coeff;
    __s16 left_coeff;

    __u16 deadband;
    __s16 center;
};
struct ff_periodic_effect {
    __u16 waveform;
    __u16 period;
    __s16 magnitude;
    __s16 offset;
    __u16 phase;

    struct ff_envelope envelope;

    __u32 custom_len;
    __s16 *custom_data;
};
struct ff_rumble_effect {
    __u16 strong_magnitude;
    __u16 weak_magnitude;
};
struct ff_effect {
    __u16 type;
    __s16 id;
    __u16 direction;
    struct ff_trigger trigger;
    struct ff_replay replay;

    union {
        struct ff_constant_effect constant;
        struct ff_ramp_effect ramp;
        struct ff_periodic_effect periodic;
        struct ff_condition_effect condition[2]; /* One for each axis */
        struct ff_rumble_effect rumble;
    } u;
};
#define EVIOCGVERSION       _IOR('E', 0x01, int)                            /* get driver version */
#define EVIOCGID            _IOR('E', 0x02, struct input_id)                /* get device ID */
#define EVIOCGREP           _IOR('E', 0x03, unsigned int[2])                /* get repeat settings */
#define EVIOCGKEYCODE		_IOR('E', 0x04, unsigned int[2])                /* get keycode */
#define EVIOCGKEYCODE_V2	_IOR('E', 0x04, struct input_keymap_entry)
#define EVIOCGNAME(len)     _IOC(_IOC_READ, 'E', 0x06, len)                 /* get device name */
#define EVIOCGPHYS(len)		_IOC(_IOC_READ, 'E', 0x07, len)                 /* get physical location */
#define EVIOCGUNIQ(len)		_IOC(_IOC_READ, 'E', 0x08, len)                 /* get unique identifier */
#define EVIOCGPROP(len)		_IOC(_IOC_READ, 'E', 0x09, len)                 /* get device properties */
#define EVIOCGMTSLOTS(len)	_IOC(_IOC_READ, 'E', 0x0a, len)
#define EVIOCGKEY(len)		_IOC(_IOC_READ, 'E', 0x18, len)                 /* get global key state */
#define EVIOCGLED(len)		_IOC(_IOC_READ, 'E', 0x19, len)                 /* get all LEDs */
#define EVIOCGSND(len)		_IOC(_IOC_READ, 'E', 0x1a, len)                 /* get all sounds status */
#define EVIOCGSW(len)		_IOC(_IOC_READ, 'E', 0x1b, len)                 /* get all switch states */
#define EVIOCGBIT(ev,len)	_IOC(_IOC_READ, 'E', 0x20 + (ev), len)          /* get event bits */
#define EVIOCGABS(abs)		_IOR('E', 0x40 + (abs), struct input_absinfo)	/* get abs value/limits */
#define EVIOCSFF            _IOW('E', 0x80, struct ff_effect)               /* send a force effect to a force feedback device */
#define EVIOCGRAB           _IOW('E', 0x90, int)                            /* Grab/Release device */
#define EVIOCREVOKE         _IOW('E', 0x91, int)                            /* Revoke device access */
#define EVIOC_MASK_SIZE(nr)	((nr) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT))
