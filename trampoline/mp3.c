#include <stddef.h>
#include "offsets.h"
#include "mp3.h"
#include "uart.h"

struct dig_transmitter_control_parameters_v1_6
{
    uint8_t phyid;
    uint8_t action;
    union
    {
        uint8_t digmode;
        uint8_t dplaneset;
    } mode_laneset;
    uint8_t lanenum;
    uint32_t symclk_10khz;
    uint8_t hpdsel;
    uint8_t digfe_sel;
    uint8_t connobj_id;
    uint8_t reserved;
    uint32_t reserved1;
};

void configure_mp3(void)
{
    int(*transmitter_control)(int, void*) = (void*)get_transmitter_control();
    int(*mp3_initialize)(int) = (void*)get_mp3_initialize();
    int(*mp3_invoke)(int, void*, void*) = (void*)get_mp3_invoke();
    struct dig_transmitter_control_parameters_v1_6 params = {};
    params.phyid = 0;
    params.action = 1; //TRANSMITTER_CONTROL_ENABLE
    params.mode_laneset.digmode = 0;
    params.lanenum = 4;
    params.symclk_10khz = 81000;
    params.hpdsel = 0;
    params.digfe_sel = 0;
    params.connobj_id = 0;
    putstring("transmitter_control... ");
    transmitter_control(0x4c, &params);
    putstring("done\r\n");
    static uint32_t mp3_req[1281], mp3_resp[1282];
    putstring("mp3_initialize... ");
    mp3_initialize(0);
    putstring("done\r\n");
    mp3_req[0] = 0;
    mp3_req[1] = 1;
    putstring("mp3_invoke(21)... ");
    mp3_invoke(21, mp3_req, mp3_resp);
    putstring("done\r\n");
    mp3_req[0] = 0;
    mp3_req[1] = 1;
    putstring("mp3_invoke(22)... ");
    mp3_invoke(22, mp3_req, mp3_resp);
    putstring("done\r\n");
}
