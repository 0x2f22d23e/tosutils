#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#pragma pack(1)

#define TRUE 1
#define FALSE 0

#define MAX_INT 0xFFFFFFFFl

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned char BOOL;

void* CAlloc(DWORD size)
{
    BYTE* res = (BYTE*)malloc(size);
    memset(res, 0, size);
    return res;
}

void Free(void* ptr)
{
    if (ptr)
        free(ptr);
}

#define ARC_MAX_BITS 12

#define CT_NONE 1
#define CT_7_BIT 2
#define CT_8_BIT 3

class CArcEntry {
public:
    CArcEntry* next;
    WORD basecode;
    BYTE ch, pad;
};

class CArcCtrl // control structure
{
public:
    DWORD src_pos, src_size, dst_pos, dst_size;
    BYTE *src_buf, *dst_buf;
    DWORD min_bits, min_table_entry;
    CArcEntry *cur_entry, *next_entry;
    DWORD cur_bits_in_use, next_bits_in_use;
    BYTE *stk_ptr, *stk_base;
    DWORD free_idx, free_limit, saved_basecode, entry_used, last_ch;
    CArcEntry compress[1 << ARC_MAX_BITS], *hash[1 << ARC_MAX_BITS];
};

class CArcCompress {
public:
    DWORD compressed_size, compressed_size_hi, expanded_size, expanded_size_hi;
    BYTE compression_type;
    BYTE body[1];
};

int Bt(int bit_num, BYTE* bit_field)
{
    bit_field += bit_num >> 3;
    bit_num &= 7;
    return (*bit_field & (1 << bit_num)) ? 1 : 0;
}

int Bts(int bit_num, BYTE* bit_field)
{
    int res;
    bit_field += bit_num >> 3;
    bit_num &= 7;
    res = *bit_field & (1 << bit_num);
    *bit_field |= (1 << bit_num);
    return (res) ? 1 : 0;
}

DWORD BFieldExtDWORD(BYTE* src, DWORD pos, DWORD bits)
{
    DWORD i, res = 0;
    for (i = 0; i < bits; i++)
        if (Bt(pos + i, src))
            Bts(i, (BYTE*)&res);
    return res;
}

void BFieldOrDWORD(BYTE* bit_field, DWORD bit_num, DWORD pattern)
{
    bit_field += bit_num >> 3;     // Increment bit_field pointer by bit_num/8
    pattern <<= bit_num & 7;       // Shift pattern bit_num % 8 to the left
    *(DWORD*)bit_field |= pattern; // OR the pattern on the bit_field.
}

void ArcEntryGet(CArcCtrl* c)
{
    DWORD i;
    CArcEntry *tmp, *tmp1;

    if (c->entry_used) {
        i = c->free_idx;

        c->entry_used = FALSE;
        c->cur_entry = c->next_entry;
        c->cur_bits_in_use = c->next_bits_in_use;
        if (c->next_bits_in_use < ARC_MAX_BITS) {
            c->next_entry = &c->compress[i++];
            if (i == c->free_limit) {
                c->next_bits_in_use++;
                c->free_limit = 1 << c->next_bits_in_use;
            }
        } else {
            do
                if (++i == c->free_limit)
                    i = c->min_table_entry;
            while (c->hash[i]);
            tmp = &c->compress[i];
            c->next_entry = tmp;
            tmp1 = (CArcEntry*)&c->hash[tmp->basecode];
            while (tmp1 && tmp1->next != tmp)
                tmp1 = tmp1->next;
            if (tmp1)
                tmp1->next = tmp->next;
        }
        c->free_idx = i;
    }
}

CArcCtrl* ArcCtrlNew(DWORD expand, DWORD compression_type)
{
    CArcCtrl* c;
    c = (CArcCtrl*)CAlloc(sizeof(CArcCtrl));
    if (expand) {
        c->stk_base = (BYTE*)malloc(1 << ARC_MAX_BITS);
        c->stk_ptr = c->stk_base;
    }
    if (compression_type == CT_7_BIT)
        c->min_bits = 7;
    else
        c->min_bits = 8;
    c->min_table_entry = 1 << c->min_bits;
    c->free_idx = c->min_table_entry;
    c->next_bits_in_use = c->min_bits + 1;
    c->free_limit = 1 << c->next_bits_in_use;
    c->saved_basecode = 0xFFFFFFFFl;
    c->entry_used = TRUE;
    ArcEntryGet(c);
    c->entry_used = TRUE;
    return c;
}

void ArcCtrlDel(CArcCtrl* c)
{
    Free(c->stk_base);
    Free(c);
}

DWORD ArcDetermineCompressionType(BYTE* src, DWORD size)
{
    while (size--)
        if (*src++ & 0x80)
            return CT_8_BIT;
    return CT_7_BIT;
}

void ArcCompressBuf(
    CArcCtrl* c)
{ // Use $LK,"CompressBuf",A="MN:CompressBuf"$() unless doing
  // more than one buf.
    CArcEntry *temp, *temp1;
    long ch, basecode;
    BYTE *src_ptr, *src_limit;

    src_ptr = c->src_buf + c->src_pos;
    src_limit = c->src_buf + c->src_size;

    if (c->saved_basecode == MAX_INT)
        basecode = *src_ptr++;
    else
        basecode = c->saved_basecode;

    while (src_ptr < src_limit && c->dst_pos + c->cur_bits_in_use <= c->dst_size) {
        ArcEntryGet(c);
    ac_start:
        if (src_ptr >= src_limit)
            goto ac_done;
        ch = *src_ptr++;
        temp = c->hash[basecode];
        if (temp)
            do {
                if (temp->ch == ch) {
                    basecode = temp - &c->compress[0];
                    goto ac_start;
                }
                temp = temp->next;
            } while (temp);
        //} while (temp=temp->next);

        BFieldOrDWORD(c->dst_buf, c->dst_pos, basecode);
        c->dst_pos += c->cur_bits_in_use;

        c->entry_used = TRUE;
        temp = c->cur_entry;
        temp->basecode = basecode;
        temp->ch = ch;
        temp1 = (CArcEntry*)&c->hash[basecode];
        temp->next = temp1->next;
        temp1->next = temp;

        basecode = ch;
    }
ac_done:
    c->saved_basecode = basecode;
    c->src_pos = src_ptr - c->src_buf;
}

BOOL ArcFinishCompression(
    CArcCtrl* c)
{ // Do closing touch on archivew ctrl struct.
    if (c->dst_pos + c->cur_bits_in_use <= c->dst_size) {
        BFieldOrDWORD(c->dst_buf, c->dst_pos, c->saved_basecode);
        c->dst_pos += c->next_bits_in_use;
        return TRUE;
    } else
        return FALSE;
}

CArcCompress* CompressBuf(BYTE* src,
    DWORD size)
{ // See ::/Demo/Dsk/SerializeTree.HC.
    CArcCompress* arc;
    DWORD size_out, compression_type = ArcDetermineCompressionType(src, size);
    CArcCtrl* c = ArcCtrlNew(FALSE, compression_type);
    c->src_size = size;
    c->src_buf = src;
    c->dst_size = (size + sizeof(CArcCompress)) << 3;
    c->dst_buf = (BYTE*)CAlloc((c->dst_size >> 3) + 4);
    c->dst_pos = sizeof(CArcCompress) << 3;
    ArcCompressBuf(c);
    if (ArcFinishCompression(c) && c->src_pos == c->src_size) {
        size_out = (c->dst_pos + 7) >> 3;
        arc = (CArcCompress*)malloc(size_out);
        memcpy(arc, c->dst_buf, size_out);
        arc->compression_type = compression_type;
        arc->compressed_size = size_out;
    } else {
        arc = (CArcCompress*)malloc(size + sizeof(CArcCompress));
        memcpy(&arc->body, src, size);
        arc->compression_type = CT_NONE;
        arc->compressed_size = size + sizeof(CArcCompress);
    }
    arc->expanded_size = size;
    Free(c->dst_buf);
    ArcCtrlDel(c);
    return arc;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Usage: %s [filename]\n", argv[0]);
        return EXIT_SUCCESS;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        printf("File not found: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    char* zfname = (char*)CAlloc(strlen(argv[1]) + 3);
    sprintf(zfname, "%s.Z", argv[1]);

    FILE* zf = fopen(zfname, "wb");
    if (!zf) {
        printf("Error opening file for write: %s\n", zfname);
        fclose(f);
    }

    printf("Compressing %s to %s ...\n", argv[1], zfname);

    fseek(f, 0, SEEK_END);
    DWORD size = ftell(f);

    printf("Original size: %d bytes\n", size);

    BYTE* buf = (BYTE*)malloc(size);
    fseek(f, 0, SEEK_SET);
    fread(buf, 1, size, f);
    fclose(f);

    CArcCompress* zbuf = CompressBuf(buf, size);
    DWORD zsize = zbuf->compressed_size;

    printf("Compressed size: %d bytes\n", zsize);

    BYTE* offset = (BYTE*)zbuf;
    fwrite(zbuf, 1, sizeof(CArcCompress) - 1, zf);
    fwrite(offset + sizeof(CArcCompress), 1, zsize - ftell(zf), zf);

    printf("\n");
    fclose(zf);
    return EXIT_SUCCESS;
}
