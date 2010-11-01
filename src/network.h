// network.h

#define MODID 404
#define NAMELONG "Network 5.5.0.3"
#define NAMESHORT "NETWORK"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

typedef struct _LOGIN_CREDENTIALS {
    BYTE initialized;
    char serial[0x41];
    char password[0x1f];

} LOGIN_CREDENTIALS;
