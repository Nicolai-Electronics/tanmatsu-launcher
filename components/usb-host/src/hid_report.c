// Parsers for raw USB HID mouse and gamepad input reports.
// Ported from https://github.com/annejan/konsool-HID

#include "hid_report.h"

/**
 * @brief Sign extend a 12 bit value to a 16 bit signed integer
 *
 * High resolution mice pack their X/Y deltas as two 12 bit signed values.
 */
static inline int16_t sign_extend_12bit(uint16_t value) {
    if (value & 0x800) {
        return (int16_t)(value | 0xF000);
    }
    return (int16_t)(value & 0x0FFF);
}

hid_mouse_report_t hid_parse_mouse_report(const uint8_t* data, int length) {
    hid_mouse_report_t report = {0};

    if (length < 3) {
        return report;
    }

    if (length <= 4) {
        // Boot protocol: buttons, X, Y and optionally a wheel byte
        report.buttons.val    = data[0];
        report.x_displacement = (int8_t)data[1];
        report.y_displacement = (int8_t)data[2];
        if (length == 4) {
            report.scroll = (int8_t)data[3];
        }
    } else if (length == 5) {
        report.buttons.val    = data[0];
        report.x_displacement = (int8_t)data[1];
        report.y_displacement = (int8_t)data[2];
        report.scroll         = (int8_t)data[3];
        report.tilt           = (int8_t)data[4];
    } else if (length < 9) {
        // Report protocol with a report ID and 12 bit displacements
        report.buttons.val    = data[1];
        report.x_displacement = sign_extend_12bit(((data[4] & 0x0F) << 8) | data[3]);
        report.y_displacement = sign_extend_12bit((data[5] << 4) | (data[4] >> 4));
        if (length >= 7) {
            report.scroll = (int8_t)data[6];
        }
        if (length == 8) {
            report.tilt = (int8_t)data[7];
        }
    } else {
        // Report protocol with a report ID and 16 bit displacements
        report.buttons.val    = data[1];
        report.x_displacement = (int16_t)((data[4] << 8) | data[3]);
        report.y_displacement = (int16_t)((data[6] << 8) | data[5]);
        report.scroll         = (int8_t)data[7];
        report.tilt           = (int8_t)data[8];
    }

    return report;
}

hid_gamepad_report_t hid_parse_gamepad_report(const uint8_t* data, int length) {
    hid_gamepad_report_t report = {0};

    if (length < 10) {
        return report;
    }

    report.report_id = data[0];

    uint8_t hat = data[1];
    uint8_t b1  = data[2];
    uint8_t b2  = data[3];

    // Hat switch, eight directions clockwise starting at up
    report.buttons.up    = (hat == 0x00 || hat == 0x01 || hat == 0x07);
    report.buttons.right = (hat == 0x01 || hat == 0x02 || hat == 0x03);
    report.buttons.down  = (hat == 0x03 || hat == 0x04 || hat == 0x05);
    report.buttons.left  = (hat == 0x05 || hat == 0x06 || hat == 0x07);

    report.buttons.a = (b2 >> 6) & 1;
    report.buttons.b = (b2 >> 5) & 1;
    report.buttons.x = (b2 >> 4) & 1;
    report.buttons.y = (b2 >> 3) & 1;

    report.buttons.l1 = (b2 >> 0) & 1;
    report.buttons.r1 = (b1 >> 7) & 1;
    report.buttons.l2 = (b2 >> 2) & 1;
    report.buttons.r2 = (b2 >> 1) & 1;
    report.buttons.l3 = (b1 >> 2) & 1;
    report.buttons.r3 = (b1 >> 3) & 1;

    report.buttons.l4     = (b1 >> 1) & 1;
    report.buttons.r4     = (b1 >> 0) & 1;
    report.buttons.select = (b1 >> 6) & 1;
    report.buttons.start  = (b1 >> 5) & 1;
    report.buttons.home   = (b1 >> 4) & 1;

    report.lx = data[4];
    report.ly = data[5];
    report.rx = data[6];
    report.ry = data[7];
    report.lt = data[8];
    report.rt = data[9];

    return report;
}
