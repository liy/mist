#include "espnow.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "sensor_reading.pb.h"

void app_main(void)
{
    // espnow_init();


    MyMessage message = MyMessage_init_zero;
    message.content.arg = &ctx;
    message.content.funcs.encode = &encode_string;

    uint8_t buffer[128];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&ostream, MyMessage_fields, &message))
    {
        const char * error = PB_GET_ERROR(&ostream);
        printf("pb_encode error: %s", error);
        return;
    }
    size_t total_bytes_encoded = ostream.bytes_written;
    printf("Encoded size: %d", total_bytes_encoded);


    // Decode the message
    callback_context_t decode_ctx;
    MyMessage decoded_message = MyMessage_init_zero;
    decoded_message.content.arg = &decode_ctx;
    decoded_message.content.funcs.decode = &decode_string;

    pb_istream_t istream = pb_istream_from_buffer(buffer, total_bytes_encoded);
    if (!pb_decode(&istream, MyMessage_fields, &decoded_message)) {
        const char *error = PB_GET_ERROR(&istream);
        printf("pb_decode error: %s\n", error);
        return;
    }

    // Print the decoded message
    printf("Decoded message:\n");
    print_my_message(&decoded_message);
}