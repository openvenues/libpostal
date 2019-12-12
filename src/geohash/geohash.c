#include "geohash.h"

// on Windows, __STDC_IEC_559__ not defined

#if defined(_MSC_VER) && (_MSC_VER <= 1500)
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int64 uint64_t;
#define UINT64_C(C) ((uint64_t) C ## ULL)
#else
#define __STDC_CONSTANT_MACROS 1
#include <stdint.h>
#endif


static inline uint16_t interleave(uint8_t upper, uint8_t lower) {
    static const uint16_t map[256] = {
        0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 
        0x0014, 0x0015, 0x0040, 0x0041, 0x0044, 0x0045, 
        0x0050, 0x0051, 0x0054, 0x0055, 0x0100, 0x0101, 
        0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 
        0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 
        0x0154, 0x0155, 0x0400, 0x0401, 0x0404, 0x0405, 
        0x0410, 0x0411, 0x0414, 0x0415, 0x0440, 0x0441, 
        0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455, 
        0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 
        0x0514, 0x0515, 0x0540, 0x0541, 0x0544, 0x0545, 
        0x0550, 0x0551, 0x0554, 0x0555, 0x1000, 0x1001, 
        0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015, 
        0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 
        0x1054, 0x1055, 0x1100, 0x1101, 0x1104, 0x1105, 
        0x1110, 0x1111, 0x1114, 0x1115, 0x1140, 0x1141, 
        0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155, 
        0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 
        0x1414, 0x1415, 0x1440, 0x1441, 0x1444, 0x1445, 
        0x1450, 0x1451, 0x1454, 0x1455, 0x1500, 0x1501, 
        0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 
        0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 
        0x1554, 0x1555, 0x4000, 0x4001, 0x4004, 0x4005, 
        0x4010, 0x4011, 0x4014, 0x4015, 0x4040, 0x4041, 
        0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055, 
        0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 
        0x4114, 0x4115, 0x4140, 0x4141, 0x4144, 0x4145, 
        0x4150, 0x4151, 0x4154, 0x4155, 0x4400, 0x4401, 
        0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415, 
        0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 
        0x4454, 0x4455, 0x4500, 0x4501, 0x4504, 0x4505, 
        0x4510, 0x4511, 0x4514, 0x4515, 0x4540, 0x4541, 
        0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555, 
        0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 
        0x5014, 0x5015, 0x5040, 0x5041, 0x5044, 0x5045, 
        0x5050, 0x5051, 0x5054, 0x5055, 0x5100, 0x5101, 
        0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 
        0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 
        0x5154, 0x5155, 0x5400, 0x5401, 0x5404, 0x5405, 
        0x5410, 0x5411, 0x5414, 0x5415, 0x5440, 0x5441, 
        0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455, 
        0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 
        0x5514, 0x5515, 0x5540, 0x5541, 0x5544, 0x5545, 
        0x5550, 0x5551, 0x5554, 0x5555
    };
    return (map[upper]<<1)+map[lower];
}


static inline void deinterleave(uint16_t interleaved, uint8_t *upper, uint8_t *lower) {
    *upper = *lower = 0;
    for (int i=7; i>=0; i--) {
        *upper = (*upper<<1) + ((interleaved>>(i*2+1))&0x01);
        *lower = (*lower<<1) + ((interleaved>>(i*2))&0x01);
    }
}


/**
 * map double[-1.0, 1.0) into uint64_t
 */
static inline int double_to_i64(double in, uint64_t *out) {
    if (in<-1.0 || 1.0<=in) {
        return 0;
    }
    union {
        double d; // assuming IEEE 754-1985 binary64. This might not be true on some CPU (I don't know which).
        // formally, we should use unsigned char for type-punning (see C99 ISO/IEC 9899:201x spec 6.2.6)
        uint64_t i64;
    } x;
    x.d = in;
    int sign = x.i64 >> 63;
    int exp = (x.i64 >> 52) & 0x7FF;
    if (exp==0) {
        *out = UINT64_C(0x8000000000000000);
        return !0;
    } else if(exp==0x7FF) {
        return 0;
    }
    
    x.i64 &= UINT64_C(0x000FFFFFFFFFFFFF);
    x.i64 |= UINT64_C(0x0010000000000000);
    int shift = exp - 0x3FF + 11;
    if (shift > 0) {
        x.i64 <<= shift;
    } else {
        x.i64 >>= -shift;
    }
    if (sign) {
        x.i64 =  UINT64_C(0x8000000000000000) - x.i64;
    } else {
        x.i64 += UINT64_C(0x8000000000000000);
    }
    *out = x.i64;
    
    return !0;
}

/**
 * map uint64_t into double[-1.0, 1.0)
 */
static inline void i64_to_double(uint64_t in, double *out) {
    union {
        double d; // assuming IEEE 754-1985 binary64. This might not be true on some CPU (I don't know which).
        // formally, we should use unsigned char for type-punning (see C99 ISO/IEC 9899:201x spec 6.2.6)
        uint64_t i64;
    } x;
    if (in==UINT64_C(0x8000000000000000)) {
        *out = 0.0;
        return;
    }
    int sign = 0;
    if (in < UINT64_C(0x8000000000000000)) {
        sign = 1; // negative. -1.0 -- 0.0
        in = UINT64_C(0x8000000000000000) - in;
    } else {
        in -= UINT64_C(0x8000000000000000);
    }
    int i;
    for (i=0;i<64;i++) {
        if (in>>(63-i)) { break; }
    }
    if (i>11) {
        x.i64 = in<<(i-11);
    } else {
        x.i64 = in>>(11-i);
    }

    x.i64 = ((UINT64_C(0x3FF) - i)<<52) + (x.i64 & UINT64_C(0x000FFFFFFFFFFFFF));
    if (sign) {
        x.i64 |= UINT64_C(0x8000000000000000);
    }
    *out = x.d;
}



static int interleaved_to_geohashstr(uint16_t *interleaved, size_t length, char *dst, size_t dst_length) {
    static const char *map="0123456789bcdefghjkmnpqrstuvwxyz";

    unsigned char *w = (unsigned char *)dst;
    uint16_t *i = interleaved;
    for (unsigned int j = 0; j < dst_length / 16; j++) {
        w[ 0] = (unsigned char)( i[0]>>11);
        w[ 1] = (unsigned char)( i[0]>>6);
        w[ 2] = (unsigned char)( i[0]>>1);
        w[ 3] = (unsigned char)((i[1]>>12) + (i[0]<<4));
        w[ 4] = (unsigned char)( i[1]>>7);
        w[ 5] = (unsigned char)( i[1]>>2);
        w[ 6] = (unsigned char)((i[2]>>13) + (i[1]<<3));
        w[ 7] = (unsigned char)( i[2]>>8);
        w[ 8] = (unsigned char)( i[2]>>3);
        w[ 9] = (unsigned char)((i[3]>>14) + (i[2]<<2));
        w[10] = (unsigned char)( i[3]>>9);
        w[11] = (unsigned char)( i[3]>>4);
        w[12] = (unsigned char)((i[4]>>15) + (i[3]<<1));
        w[13] = (unsigned char)( i[4]>>10);
        w[14] = (unsigned char)( i[4]>>5);
        w[15] = (unsigned char)( i[4]);
        i += 5;
        w += 16;
    }

    for (unsigned int j=0; j < dst_length % 16; j++) {
        if      (j ==  0) w[ 0] = (unsigned char)( i[0]>>11);
        else if (j ==  1) w[ 1] = (unsigned char)( i[0]>>6);
        else if (j ==  2) w[ 2] = (unsigned char)( i[0]>>1);
        else if (j ==  3) w[ 3] = (unsigned char)((i[1]>>12) + (i[0]<<4));
        else if (j ==  4) w[ 4] = (unsigned char)( i[1]>>7);
        else if (j ==  5) w[ 5] = (unsigned char)( i[1]>>2);
        else if (j ==  6) w[ 6] = (unsigned char)((i[2]>>13) + (i[1]<<3));
        else if (j ==  7) w[ 7] = (unsigned char)( i[2]>>8);
        else if (j ==  8) w[ 8] = (unsigned char)( i[2]>>3);
        else if (j ==  9) w[ 9] = (unsigned char)((i[3]>>14) + (i[2]<<2));
        else if (j == 10) w[10] = (unsigned char)( i[3]>>9);
        else if (j == 11) w[11] = (unsigned char)( i[3]>>4);
        else if (j == 12) w[12] = (unsigned char)((i[4]>>15) + (i[3]<<1));
        else if (j == 13) w[13] = (unsigned char)( i[4]>>10);
        else if (j == 14) w[14] = (unsigned char)( i[4]>>5);
        else if (j == 15) w[15] = (unsigned char)( i[4]);
    }


    for (unsigned int j = 0; j < dst_length; j++) {
        dst[j] = map[dst[j] & 0x1F];
    }
    return GEOHASH_OK;
}


/*
  latitude must be in [-90.0, 90.0) and longitude must be in [-180.0 180.0)
*/
int geohash_encode(double latitude, double longitude, char *r, size_t capacity) {
    uint64_t lat64, lon64;
    uint16_t interleaved[9];
    if (capacity > 27) {
        return GEOHASH_INVALIDARGUMENT;
    }
    char lr[capacity];
    memset(interleaved, 0, sizeof(interleaved));

    while (longitude < -180.0) longitude += 360.0;
    while (longitude >= 180.0) longitude -= 360.0;

    if (!double_to_i64(latitude/90.0, &lat64) || !double_to_i64(longitude/180.0, &lon64)) {
        return GEOHASH_INVALIDARGUMENT;
    }
    for (int i=0; i<8; i++) {
        interleaved[7-i] = interleave((uint8_t)(lon64>>(i*8)), (uint8_t)(lat64>>(i*8)));
    }
    
    int ret = GEOHASH_OK;
    if ((ret = interleaved_to_geohashstr(interleaved, 8, lr, capacity - 1)) != GEOHASH_OK) {
        return ret;
    }
    lr[capacity - 1] = '\0';
    
    if (0 < capacity) {
        memcpy(r, (const char*)lr, capacity-1);
        r[capacity - 1] = '\0';
    }
    return GEOHASH_OK;
}


/**
 * handle geohash string decoding operation
 */
static int geohashstr_to_interleaved(char *r, size_t length, uint16_t *interleaved, size_t dst_count) {
    static const unsigned char map[128] = {
          '|',  '|',  '|',  '|',  '|',  '|',  '|',  '|',
          '|',  '|',  '|',  '|',  '|',  '|',  '|',  '|',
          '|',  '|',  '|',  '|',  '|',  '|',  '|',  '|',
          '|',  '|',  '|',  '|',  '|',  '|',  '|',  '|',
          '|',  '|',  '|',  '|',  '|',  '|',  '|',  '|',
          '|',  '|',  '|',  '|',  '|',  '|',  '|',  '|',
            0,    1,    2,    3,    4,    5,    6,    7,
            8,    9,  '|',  '|',  '|',  '|',  '|',  '|',
          '|',  '|', 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
         0x10,  '|', 0x11, 0x12,  '|', 0x13, 0x14,  '|',
         0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
         0x1D, 0x1E, 0x1F,  '|',  '|',  '|',  '|',  '|',
          '|',  '|', 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
         0x10,  '|', 0x11, 0x12,  '|', 0x13, 0x14,  '|',
         0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
         0x1D, 0x1E, 0x1F,  '|',  '|',  '|',  '|',  '|',
    };
    for (unsigned int i=0; i<length; i++) {
        if (r[i]==0) {
            length = i;
            break;
        } else if(r[i]<0 || map[(unsigned char)r[i]]=='|') {
            return GEOHASH_INVALIDCODE;
        }
    }
    if (dst_count*16 < length*5) {
        return GEOHASH_INTERNALERROR;
    }
    for (unsigned int j=0; j<dst_count; j++) {
        interleaved[j]=0;
    }
    
    uint16_t *i = interleaved;
    unsigned char *c = (unsigned char*)r;
    for (unsigned int j=0; j<length/16; j++) {
        i[0] = (map[c[ 0]]<<11) + (map[c[ 1]]<< 6) + (map[c[ 2]]<<1) + (map[c[ 3]]>>4);
        i[1] = (map[c[ 3]]<<12) + (map[c[ 4]]<< 7) + (map[c[ 5]]<<2) + (map[c[ 6]]>>3);
        i[2] = (map[c[ 6]]<<13) + (map[c[ 7]]<< 8) + (map[c[ 8]]<<3) + (map[c[ 9]]>>2);
        i[3] = (map[c[ 9]]<<14) + (map[c[10]]<< 9) + (map[c[11]]<<4) + (map[c[12]]>>1);
        i[4] = (map[c[12]]<<15) + (map[c[13]]<<10) + (map[c[14]]<<5) + (map[c[15]]>>0);
        i+=5;
        c+=16;
    }
    for (unsigned int j=0; j<length%16; j++) {
        if(j== 0) i[0]  = map[c[ 0]]<<11;
        if(j== 1) i[0] += map[c[ 1]]<< 6;
        if(j== 2) i[0] += map[c[ 2]]<< 1;
        if(j== 3) {
            i[0] += map[c[ 3]]>> 4;
            i[1]  = map[c[ 3]]<<12;
        }
        if(j== 4) i[1] += map[c[ 4]]<< 7;
        if(j== 5) i[1] += map[c[ 5]]<< 2;
        if(j== 6) {
            i[1] += map[c[ 6]]>> 3;
            i[2]  = map[c[ 6]]<<13;
        }
        if(j== 7) i[2] += map[c[ 7]]<< 8;
        if(j== 8) i[2] += map[c[ 8]]<< 3;
        if(j== 9) {
            i[2] += map[c[ 9]]>> 2;
            i[3]  = map[c[ 9]]<<14;
        }
        if(j==10) i[3] += map[c[10]]<< 9;
        if(j==11) i[3] += map[c[11]]<< 4;
        if(j==12) {
            i[3] += map[c[12]]>> 1;
            i[4]  = map[c[12]]<<15;
        }
        if(j==13) i[4] += map[c[13]]<<10;
        if(j==14) i[4] += map[c[14]]<< 5;
        if(j==15) i[4] += map[c[15]]>> 0;
    }
    return GEOHASH_OK;
}


/*
   (latitude, longitude) will be that of south west point.
*/
int geohash_decode(char *r, size_t length, double *latitude, double *longitude) {
    uint16_t intr_auto[8];
    uint16_t *interleaved = intr_auto;
    size_t intr_length = length*5/16+1;
    int intr_free = 0;
    if (intr_length > 8) {
        interleaved = (uint16_t*)malloc(sizeof(uint16_t)*intr_length);
        if (!interleaved) {
            return GEOHASH_NOMEMORY;
        }
        intr_free = 1;
    } else {
        intr_length = 8;
    }
    int ret = GEOHASH_OK;
    if ((ret=geohashstr_to_interleaved(r, length, interleaved, intr_length)) != GEOHASH_OK) {
        return ret;
    }
    uint64_t lat64=0;
    uint64_t lon64=0;
    for (int i=0; i<8; i++) {
        uint8_t upper, lower;
        deinterleave(interleaved[i], &upper, &lower);
        lon64 = (lon64<<8)+upper;
        lat64 = (lat64<<8)+lower;
    }
    if (intr_free) {
        free(interleaved);
    }
    
    double t;
    
    i64_to_double(lat64, &t);
    *latitude = t*90.0;
    
    i64_to_double(lon64, &t);
    *longitude = t*180.0;
    
    return GEOHASH_OK;
}



/**
 * compare two uint8_t array of variable sized integers.
 */
static int uint8s_cmp(uint8_t *src, uint8_t *dst, size_t length) {
    if (length==0) { return 0; }
    unsigned int i=0;
    for (i=0; i<(length-1)/8; i++) {
        if (src[i] != dst[i]) {
            return (int)(src[i] - dst[i]);
        }
    }
    uint8_t cell_offset = (8-length%8)%8;
    return (int)((src[i]>>cell_offset) - (dst[i]>>cell_offset));
}

/**
 * plus minus operations for uint8_t array of variable sized integer.
 */
static int uint8s_plus_minus(uint8_t *src, uint8_t *dst, size_t length, int plus) {
    if (length==0) {
        return 0;
    }
    unsigned int cell = (length-1)/8;
    for (unsigned int i=0; i<cell; i++) {
        dst[cell] = src[cell];
    }
    uint8_t cell_offset = (8-length%8)%8;
    int up = 1;
    while (up) {
        uint8_t t;
        up = 0;
        if (plus) {
            t = src[cell] + (1<<cell_offset);
            if ((src[cell]&0x80) && !(t&0x80)) {
                up = 1;
            }
        } else {
            t = src[cell] - (1<<cell_offset);
            if (!(src[cell]&0x80) && (t&0x80)) {
                up = 1;
            }
        }
        dst[cell] = t;
        cell_offset = 0;
        if (cell == 0) {
            break;
        }
        cell--;
    }
    return !0;
}

static int neighbors(uint16_t *interleaved, size_t bitlength, uint16_t *dst, size_t dst_length, size_t *dst_count) {
    size_t interleaved_length = 0;
    while (interleaved_length*16 < bitlength) {
        interleaved_length++;
    }
    if (dst_length<interleaved_length*8) {
        return GEOHASH_INTERNALERROR;
    }
    uint8_t latlons[interleaved_length*6];

    uint8_t *lat_8s = latlons;
    uint8_t *lon_8s = latlons + interleaved_length*3;
    
    unsigned int lat_len = bitlength/2;
    unsigned int lon_len = bitlength/2+bitlength%2;
    
    for (unsigned int i=0; i<interleaved_length; i++) {
        deinterleave(interleaved[i], lon_8s+i, lat_8s+i);
        lat_8s[i+interleaved_length*2] = lat_8s[i+interleaved_length] = lat_8s[i];
        lon_8s[i+interleaved_length*2] = lon_8s[i+interleaved_length] = lon_8s[i];
    }

    uint8_t *lats[3] = {lat_8s, lat_8s+interleaved_length, lat_8s+interleaved_length*2};
    uint8_t *lons[3] = {lon_8s, lon_8s+interleaved_length, lon_8s+interleaved_length*2};
    
    if (uint8s_plus_minus(lats[0], lats[1], lat_len, 0)) {
        if ((lats[1][0]&0x80) && !(lats[0][0]&0x80)) {
            for (unsigned int i=0; i<interleaved_length; i++) {
                lats[1][i] = lats[0][i]; // this cause skip
            }
        }
    }
    if (uint8s_plus_minus(lats[0], lats[2], lat_len, 1)) {
        if (!(lats[2][0]&0x80) && (lats[0][0]&0x80)) {
            for (unsigned int i=0; i<interleaved_length; i++) {
                lats[2][i] = lats[1][i]; // this cause skip
            }
        }
    }
    uint8s_plus_minus(lons[0], lons[1], lon_len, 0);
    uint8s_plus_minus(lons[0], lons[2], lon_len, 1);
    
    size_t neighbour_count = 0;
    for (unsigned int i=0;i<3;i++) {
        if (i>0 && uint8s_cmp(lats[i-1], lats[i], lat_len)==0) {
            continue;
        }
        for (int j=0;j<3;j++) {
            if (j>0 && uint8s_cmp(lons[j-1], lons[j], lon_len)==0) {
                continue;
            }
            if (i==0 && j==0) {
                continue;
            }
            
            for (unsigned int k=0; k<interleaved_length; k++) {
                dst[interleaved_length*neighbour_count+k] = interleave(lons[j][k], lats[i][k]);
            }
            neighbour_count++;
        }
    }
    if (dst_count) {
        *dst_count = neighbour_count;
    }
    return GEOHASH_OK;
}

int geohash_neighbors(char *hashcode, char *dst, size_t dst_length, int *string_count) {
    int ret = GEOHASH_OK;
    size_t hashcode_length = strlen(hashcode);
    size_t interleaved_length = 0;
    while (interleaved_length*16 < hashcode_length*5) {
        interleaved_length++;
    }

    uint16_t interleaved[interleaved_length * 9];

    if ((ret=geohashstr_to_interleaved(hashcode, hashcode_length, interleaved, interleaved_length)) != GEOHASH_OK) {
        return ret;
    }
    
    size_t dst_count = 0;
    uint16_t *intr_dst = interleaved + interleaved_length;
    if ((ret = neighbors(interleaved, hashcode_length*5, intr_dst, interleaved_length*8, &dst_count) != GEOHASH_OK)) {
        return ret;
    }
    
    size_t blen = hashcode_length;

    char buffer[blen];
    memset(buffer, 0, blen);
    
    size_t t = hashcode_length + 1;
    for (unsigned int i=0; i<dst_count; i++) {
        if ((ret = interleaved_to_geohashstr(intr_dst+i*interleaved_length, interleaved_length, buffer, blen)) != GEOHASH_OK) {
            return ret;
        }
        buffer[hashcode_length] = '\0';
        memcpy(dst+i*t, buffer, t);
    }

    if (string_count) {
        *string_count = dst_count;
    }
    
    return GEOHASH_OK;
}
