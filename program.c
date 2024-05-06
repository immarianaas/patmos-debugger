#include <machine/spm.h>
#include <machine/exceptions.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define G_REGISTERS (32 + 6)
#define GDB_BUF_MAX (G_REGISTERS * 8 + 1)
#define NUM_INSTRUCTIONS 50
#define TRAP16 0x05800010

#define UART_STATUS *((volatile _IODEV int *)(PATMOS_IO_UART + 0x0))
#define UART_DATA *((volatile _IODEV int *)(PATMOS_IO_UART + 0x4))

static const char hexchars[] = "0123456789abcdef";
void intr_handler(void) __attribute__((naked));

/**
 * Structure to help with sending/receiving packets.
 */
struct rsp_buf
{
    char data[GDB_BUF_MAX];
    int len;
};

/* These arrays function as a lookup table */
int addresses[NUM_INSTRUCTIONS];
int replacedMem[NUM_INSTRUCTIONS];
int currIndex = 0;

/**
 * Given an address, returns the instruction previously saved in the "lookup table".
 */
int getMem(int originalAddress)
{
    for (int i = 0; i < NUM_INSTRUCTIONS; i++)
    {
        if (addresses[i] == originalAddress)
            return replacedMem[i];
    }
    return -1;
}

/**
 * Save an instruction for the given address in the "lookup table".
 */
void saveMem(int originalAddress, int originalMemory)
{
    addresses[currIndex] = originalAddress;
    replacedMem[currIndex] = originalMemory;
    currIndex = (currIndex + 1) % NUM_INSTRUCTIONS;
}

/**
 * Read a char from UART.
 */
int read_char()
{
    while ((UART_STATUS & 0x02) == 0)
    {
    }
    return UART_DATA;
}

/**
 * Write a char to UART.
 */
void write_char(char ch)
{
    while (UART_STATUS == 0)
    {
    }
    UART_DATA = ch;
}

/**
 * Convert a hexadecimal char to a decimal value.
 */
static int
hex2dec(char c)
{
    return ((c >= 'a') && (c <= 'f')) ? c - 'a' + 10 : ((c >= '0') && (c <= '9')) ? c - '0'
                                                   : ((c >= 'A') && (c <= 'F'))   ? c - 'A' + 10
                                                                                  : -1;
}

/**
 * Receives a full packet from the host.
 * Returns with '+' or '-' to host depending on whether the checksum is valid or not.
 * Returns with the message without the leading and trailing characters.
 */
static struct rsp_buf *get_packet()
{

    static struct rsp_buf buf;

    while (1)
    {
        unsigned char checksum;
        int count;

        int ch = read_char();

        // buffer received should start with $
        while (ch != '$')
        {
            ch = read_char(); // keep trying
        }

        // at this point, ch == '$'

        checksum = 0;
        count = 0;

        while (count < GDB_BUF_MAX - 1)
        {
            ch = read_char();

            if (ch == '$') // start again
            {
                checksum = 0;
                count = 0;
                continue;
            }

            if (ch == '#') // finished
                break;

            checksum = checksum + (unsigned char)ch;
            buf.data[count] = (char)ch;
            count++;
        }

        buf.data[count] = 0;
        buf.len = count;

        if ('#' == ch)
        {
            unsigned char checksum_recv;

            ch = read_char();
            checksum_recv = hex2dec(ch) << 4;
            ch = read_char();
            checksum_recv += hex2dec(ch);

            if (checksum != checksum_recv)
            {
                write_char('-');
            }
            else
            {
                write_char('+');
                break;
            }
        }
    }
    return &buf;
}

void put_packet(struct rsp_buf *buf)
{
    int ch;

    // Construct '$<message>#<checksum>'.
    // Will repeat until the host acknowledges the packet.
    do
    {
        unsigned int checksum = 0;
        int count = 0;

        write_char('$');

        for (count = 0; count < buf->len; count++)
        {
            unsigned char ch = buf->data[count];

            // Check for escaped chars
            if (('$' == ch) || ('#' == ch) || ('*' == ch) || ('}' == ch))
            {
                ch ^= 0x20;
                checksum += (unsigned char)'}';
                write_char('}');
            }

            checksum += ch;
            write_char(ch);
        }

        write_char('#');

        unsigned int sum = checksum % 256;
        char checksum_str[3];
        sprintf(checksum_str, "%02x", sum);
        for (int i = 0; i < 2; i++)
        {
            write_char(checksum_str[i]);
        }

        // check connection
        ch = read_char();
        if (-1 == ch)
            return;

    } while ('+' != ch);
}

/**
 * Checks if `str` starts with the substring `substr`.
 */
int starts_with_substr(char *str, char *substr)
{
    return 0 == strncmp(substr, str, strlen(substr));
}

/**
 * Given a NULL terminated string, creates a packet with it as a message,
 * and sends it over UART.
 */
void put_str_packet(char *msg, struct rsp_buf *buf)
{
    int len = strlen(msg);
    strncpy(buf->data, msg, len);
    buf->data[len] = 0;
    buf->len = len;

    put_packet(buf);
}

/**
 * Handles the response to 'g' packets, by fetching the register values from the
 * stack cache, and computing the PC from $sxb and $sxo. 
 * Sends the info to the host.
*/
void send_registers(struct rsp_buf *buf)
{
    char reply[GDB_BUF_MAX];
    int vals[G_REGISTERS];

    asm volatile(
        "lws  $r10  = [3];"
        "lws  $r11  = [4];"
        "lws  $r12  = [5];"
        "lws  $r13  = [6];"
        "lws  $r14  = [7];"
        "lws  $r15  = [8];"
        "lws  $r16  = [9];"
        "lws  $r17 = [10];"
        "lws  $r18 = [11];"
        "lws  $r19 = [12];"
        "lws  $r20 = [13];"
        "mov %0  = $r10  ;"
        "mov %1  = $r11  ;"
        "mov %2  = $r12  ;"
        "mov %3  = $r13  ;"
        "mov %4  = $r14  ;"
        "mov %5  = $r15  ;"
        "mov %6  = $r16  ;"
        "mov %7  = $r17  ;"
        "mov %8  = $r18  ;"
        "mov %9  = $r19  ;"
        "mov %10 = $r20  ;"
        : "=r"(vals[3]), "=r"(vals[4]), "=r"(vals[5]),
          "=r"(vals[6]), "=r"(vals[7]), "=r"(vals[8]),
          "=r"(vals[9]), "=r"(vals[10]), "=r"(vals[11]),
          "=r"(vals[12]), "=r"(vals[13])
        :
        : "$r10", "$r11", "$r12", "$r13", "$r14", "$r15", "$r16", "$r17", "$r18", "$r19", "$r20");

    asm volatile(
        "lws  $r10  = [14];"
        "lws  $r11  = [15];"
        "lws  $r12  = [16];"
        "lws  $r13  = [17];"
        "lws  $r14  = [18];"
        "lws  $r15  = [19];"
        "lws  $r16  = [20];"
        "lws  $r17  = [21];"
        "lws  $r18  = [22];"
        "lws  $r19  = [23];"
        "lws  $r20  = [24];"
        "mov %0  = $r10   ;"
        "mov %1  = $r11   ;"
        "mov %2  = $r12   ;"
        "mov %3  = $r13   ;"
        "mov %4  = $r14   ;"
        "mov %5  = $r15   ;"
        "mov %6  = $r16   ;"
        "mov %7  = $r17   ;"
        "mov %8  = $r18   ;"
        "mov %9  = $r19   ;"
        "mov %10  = $r20  ;"
        : "=r"(vals[14]), "=r"(vals[15]), "=r"(vals[16]),
          "=r"(vals[17]), "=r"(vals[18]), "=r"(vals[19]),
          "=r"(vals[20]), "=r"(vals[21]), "=r"(vals[22]),
          "=r"(vals[23]), "=r"(vals[24])
        :
        : "$r10", "$r11", "$r12", "$r13", "$r14", "$r15", "$r16", "$r17", "$r18", "$r19", "$r20");

    asm volatile(
        "lws  $r10  = [25];"
        "lws  $r11  = [26];"
        "lws  $r12  = [27];"
        "lws  $r13  = [28];"
        "lws  $r14  = [29];"
        "lws  $r15  = [30];"
        "mov %0  = $r10   ;"
        "mov %1  = $r11   ;"
        "mov %2  = $r12   ;"
        "mov %3  = $r13   ;"
        "mov %4  = $r14   ;"
        "mov %5  = $r15   ;"

        "lwc  $r16 = [$r31 + 0];" // $r1
        "lwc  $r17 = [$r31 + 1];" // $r2
        "add  $r18 = $r31, 12  ;" // $r31
        "mov %6  = $r16   ;"      // $r1
        "mov %7  = $r17   ;"      // $r2
        "mov %8  = $r18   ;"      // $r31
        : "=r"(vals[25]), "=r"(vals[26]), "=r"(vals[27]),
          "=r"(vals[28]), "=r"(vals[29]), "=r"(vals[30]),
          "=r"(vals[1]), "=r"(vals[2]), "=r"(vals[31])
        :
        : "$r10", "$r11", "$r12", "$r13", "$r14", "$r15", "$r16", "$r17", "$r18");

    vals[0] = 0;

    for (int i = 0; i < 32; ++i)
    {
        sprintf(&reply[i * 8], "%08x", vals[i]);
    }

    sprintf(&reply[32 * 8], "xxxxxxxx"); // sr
    sprintf(&reply[33 * 8], "xxxxxxxx"); // lo
    sprintf(&reply[34 * 8], "xxxxxxxx"); // hi
    sprintf(&reply[35 * 8], "xxxxxxxx"); // bad
    sprintf(&reply[36 * 8], "xxxxxxxx"); // cause

    int sxb;
    int sxo;
    asm volatile(
        "mfs %0 = $sxb\n\t"
        "mfs %1 = $sxo\n\t"
        : "=r"(sxb), "=r"(sxo)::"$s9", "$s10");

    int pc = sxb + sxo - 4;
    sprintf(&reply[37 * 8], "%08x", pc); // pc

    put_str_packet(reply, buf);
}

/**
 * Given a string, returns the index of where the ',' is.
 * The string must have a ','!
*/
int get_comma_index(char *msg)
{
    int i = 0;
    while (msg[i] != ',')
    {
        ++i;
    }
    return i;
}

/**
 * Given a 'm', 'Z0' or 'z0' packet, which always contains an address,
 * this function extracts this address and returns it as an int.
*/
int get_address(char *msg)
{
    int begin, size;
    char pointer[6];

    if (starts_with_substr(msg, "m")) // m24b40,4
    {
        begin = 1;
        // size = 5;
        size = get_comma_index(msg + begin);
    }
    else if (starts_with_substr(msg, "Z") || starts_with_substr(msg, "z"))
    {
        begin = 3;
        size = get_comma_index(msg + begin); // 5;
    }

    strncpy(pointer, msg + begin, size);
    pointer[size] = 0;
    int addrInt = strtol(pointer, NULL, 16);
    return addrInt;
}

/**
 * Function that is triggered by an interrupt.
 * Will exchange messages with the host, and react to the
 * different packets received.
*/
void handle_communication()
{
    struct rsp_buf buf;
    put_str_packet("S05", &buf);

    while (1)
    {
        struct rsp_buf *buf = get_packet();

        if (starts_with_substr(buf->data, "qSupported"))
        {
            char reply[GDB_BUF_MAX];
            sprintf(reply, "PacketSize=%x", GDB_BUF_MAX);
            put_str_packet(reply, buf);
            continue;
        }

        if (starts_with_substr(buf->data, "H"))
        {
            put_str_packet("OK", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qTStatus"))
        {
            // we don't support tracing
            put_str_packet("", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "?"))
        {
            put_str_packet("S05", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qfThreadInfo"))
        {
            put_str_packet("m1", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qsThreadInfo"))
        {
            put_str_packet("l", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qAttached"))
        {
            put_str_packet("1", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qC"))
        {
            // same thread
            put_str_packet("", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qOffsets"))
        {
            put_str_packet("Text=0;Data=0;Bss=0", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "qSymbol"))
        {
            put_str_packet("", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "g"))
        {
            send_registers(buf);
            continue;
        }

        if (starts_with_substr(buf->data, "m"))
        {
            int addrInt = get_address(buf->data);
            volatile _UNCACHED int *addr = (volatile _UNCACHED int *)addrInt;

            char reply[GDB_BUF_MAX];
            sprintf(reply, "%08x", *addr);
            put_str_packet(reply, buf);
            continue;
        }

        if (starts_with_substr(buf->data, "Z"))
        {
            int addrInt = get_address(buf->data);
            volatile _UNCACHED int *addr = NULL;
            addr = (volatile _UNCACHED int *)addrInt;

            while (1)
            {
                if ((*addr & 0x80000000) != 0)
                    addr += 2;
                else if ((*(addr - 1) & 0x80000000) != 0)
                    addr += 1;
                else
                    break;
            }

            int original = (int)*addr;
            saveMem(addrInt, original);
            *addr = TRAP16;
            inval_mcache();
            put_str_packet("OK", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "z"))
        {
            int addrInt = get_address(buf->data);
            volatile _UNCACHED int *addr = (volatile _UNCACHED int *)addrInt;

            while (1)
            {
                if ((*addr & 0x80000000) != 0)
                    addr += 2;
                else if ((*(addr - 1) & 0x80000000) != 0)
                    addr += 1;
                else
                    break;
            }

            *addr = getMem(addrInt);
            inval_mcache();
            put_str_packet("OK", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "p"))
        {
            put_str_packet("xxxxxxxx", buf);
        }

        // handle unknown 'v' packets (and vMustReplyEmpty)
        if (starts_with_substr(buf->data, "v"))
        {
            put_str_packet("", buf);
            continue;
        }

        if (starts_with_substr(buf->data, "D"))
        {
            // DEATCH
            put_str_packet("OK", buf);
            return;
        }

        if (starts_with_substr(buf->data, "c"))
        {
            // RESUME EXECUTION
            return;
        }
    }
}

/**
 * Triggers an interrupt.
*/
void breakpoint()
{
    asm("trap 16");
}

/**
 * Interrupt handler.
 * Function called when there is an interrupt.
 * Saves the status of the registers and calls the function 
 * to communicate with the host.
*/
void intr_handler(void)
{
    exc_prologue();
    // give control to GDB
    handle_communication();
    exc_epilogue();
}

/**
 * Configures the interrupt handler for all interrupts.
*/
void set_debug_traps()
{
    for (unsigned i = 0; i < 32; i++)
        exc_register(i, &intr_handler);

    // unmask interrupts
    intr_unmask_all();
    // clear pending flags
    intr_clear_all_pending();
    // enable interrupts
    intr_enable();
}

/**
 * Sends a NULL-terminated string over UART.
*/
int send_string_uart(char *ch)
{
    int index = 0;
    char c = ch[index++];
    while (c != 0)
    {
        write_char(c);
        c = ch[index++];
    }
    return 0;
}

/**
 * Main function of the program.
*/
int main()
{
    set_debug_traps();
    breakpoint();

    int a = 2;
    int c = 5;
    int result = (a + c) * a;
    char string[50];
    sprintf(string, "Hello world! result=%d", result);
    send_string_uart(string);
}