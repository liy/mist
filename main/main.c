#include "espnow.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "message.pb.h"

// Function to print the contents of MyMessage
void print_my_message(const MyMessage *msg) {
    printf("Text: %s\n", (char *)msg->content.arg);
}

typedef struct
{
    char* content;
}
callback_context_t;

bool encode_string(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    // ...and you always cast to the same pointer type, reducing
    // the chance of mistakes
    callback_context_t * ctx = (callback_context_t *)(*arg);

    if (!pb_encode_tag_for_field(stream, field))
        return false;

    return pb_encode_string(stream, (uint8_t*)ctx->content, strlen(ctx->content));
}

bool decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    // uint8_t buffer[1024] = {0};

    // uint8_t total_bytes_encoded = stream->bytes_left;
    // /* We could read block-by-block to avoid the large buffer... */
    // if (stream->bytes_left > sizeof(buffer) - 1)
    //     return false;
    
    // if (!pb_read(stream, buffer, stream->bytes_left))
    //     return false;

    pb_field_iter_t iter;

    if (!pb_field_iter_begin(&iter, MyMessage_fields, message))
        return false;

    do
    {
        if (iter.submsg_desc == messagetype)
        {
            /* This is our field, encode the message using it. */
            if (!pb_encode_tag_for_field(stream, &iter))
                return false;
            
            return pb_encode_submessage(stream, messagetype, message);
        }
    } while (pb_field_iter_next(&iter));


    return true;
}

void app_main(void)
{
    // espnow_init();

    callback_context_t ctx = { .content = "Hello world" };

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