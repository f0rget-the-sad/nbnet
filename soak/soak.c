#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <time.h>
#endif

#include "soak.h"

#ifdef NBN_DRIVER_UDP_IMPL
#include "../net_drivers/udp.h"
#endif

#ifdef NBN_DRIVER_UDP_SDL_NET_IMPL
#include "../net_drivers/udp_sdl_net.h"
#endif

enum
{
    OPT_MESSAGES_COUNT,
    OPT_MIN_PACKET_LOSS,
    OPT_MAX_PACKET_LOSS,
    OPT_PACKET_DUPLICATION,
    OPT_PING,
    OPT_JITTER
};

static bool running = true;
static SoakOptions soak_options = {0};

void Soak_Init(void)
{
    srand(SOAK_SEED);

    NBN_RegisterMessage(SOAK_MESSAGE, SoakMessage_Create, SoakMessage_Serialize, NULL);
    NBN_RegisterChannel(NBN_CHANNEL_RELIABLE_ORDERED, SOAK_CHAN_RELIABLE_ORDERED_1);
    NBN_RegisterChannel(NBN_CHANNEL_RELIABLE_ORDERED, SOAK_CHAN_RELIABLE_ORDERED_2);
    NBN_RegisterChannel(NBN_CHANNEL_RELIABLE_ORDERED, SOAK_CHAN_RELIABLE_ORDERED_3);
    NBN_RegisterChannel(NBN_CHANNEL_RELIABLE_ORDERED, SOAK_CHAN_RELIABLE_ORDERED_4);
}

int Soak_ReadCommandLine(int argc, char *argv[])
{
#ifdef NBN_GAME_CLIENT
    if (argc < 2)
    {
        printf("Usage: client --message_count=<value> [--min_packet_loss=<value>] [--max_packet_loss=<value>] \
            [--packet_duplication=<value>] [--ping=<value>]\n");

        return -1;
    }
#endif

    int opt;
    int option_index;
    struct option long_options[] = {
        { "messages_count", required_argument, NULL, OPT_MESSAGES_COUNT },
        { "min_packet_loss", required_argument, NULL, OPT_MIN_PACKET_LOSS },
        { "max_packet_loss", required_argument, NULL, OPT_MAX_PACKET_LOSS },
        { "packet_duplication", required_argument, NULL, OPT_PACKET_DUPLICATION },
        { "ping", required_argument, NULL, OPT_PING },
        { "jitter", required_argument, NULL, OPT_JITTER }
    };

    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
#ifdef NBN_GAME_CLIENT
        case OPT_MESSAGES_COUNT:
            soak_options.messages_count = atoi(optarg);
            break;
#endif

        case OPT_MIN_PACKET_LOSS:
            soak_options.min_packet_loss = atof(optarg);
            break;

        case OPT_MAX_PACKET_LOSS:
            soak_options.max_packet_loss = atof(optarg);
            break;

        case OPT_PACKET_DUPLICATION:
            soak_options.packet_duplication = atof(optarg);
            break;

        case OPT_PING:
            soak_options.ping = atoi(optarg);
            break;

        case OPT_JITTER:
            soak_options.jitter = atoi(optarg);
            break;

        case '?':
            return -1;
        
        default:
            return -1;
        }
    }

    return 0;
}

int Soak_MainLoop(int (*Tick)(void))
{
    while (running)
    {
        if (Tick() < 0)
            return 1;

#ifdef __EMSCRIPTEN__
    emscripten_sleep(1.f / SOAK_TICK_RATE);
#else
    long nanos = (1.f / SOAK_TICK_RATE) * 1e9;
    struct timespec t = { .tv_sec = nanos / 999999999, .tv_nsec = nanos % 999999999 };

    nanosleep(&t, &t);
#endif
    }

    return 0;
}

void Soak_Stop(void)
{
    running = false;

    log_info("Soak test stopped");
}

SoakOptions Soak_GetOptions(void)
{
    return soak_options;
}

SoakMessage *SoakMessage_Create(void)
{
    SoakMessage *msg = malloc(sizeof(SoakMessage));

    msg->id = 0;

    return msg; 
}

int SoakMessage_Serialize(SoakMessage *msg, NBN_Stream *stream)
{
    SERIALIZE_UINT(msg->id, 0, UINT_MAX);
    SERIALIZE_UINT(msg->data_length, 1, SOAK_MESSAGE_MAX_DATA_LENGTH);
    SERIALIZE_BYTES(msg->data, msg->data_length);

    return 0;
}

void Soak_Debug_PrintAddedToRecvQueue(NBN_Connection *conn, NBN_Message *msg)
{
    if (msg->header.type == NBN_MESSAGE_CHUNK_TYPE)
    {
        log_debug("Soak message chunk added to recv queue");
    }
    else
    {
        SoakMessage *soak_message = (SoakMessage *)msg->data;

        log_debug("Soak message added to recv queue (conn id: %d, msg id: %d, soak msg id: %d)",
                conn->id, msg->header.id, soak_message->id);
    }
}
