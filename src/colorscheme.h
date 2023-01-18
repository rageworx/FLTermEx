#ifndef __COLORSCHEME_H__
#define __COLORSCHEME_H__

#include <cstdint>
#include <vector>

#define COLORSCHEME_NAME_LEN        (80)

typedef struct _SchemeColor
{
    char    name[COLORSCHEME_NAME_LEN];
    float   alpha;
    float   red;
    float   green;
    float   blue;
}SchemeColor;

class ColorScheme
{
    public:
        ColorScheme( uint16_t bits = 8, const char* itcfn = nullptr );
        ~ColorScheme();

    public:
        // notice - 
        // RGBA16 meaning HDR10 or HDR10+ not padded color order. 
        uint32_t Scheme2RGBA8( SchemeColor &scol );
        uint64_t Scheme2RGBA16( SchemeColor &scol );

    public:
        bool Load( const char* itcfn = nullptr );
        void Unload();
        size_t size();

    public:
        size_t findKey( const char* kn, bool exac = false );
        bool   getColor( size_t ki, SchemeColor &scol );
        bool   getColor( const char* kn, SchemeColor &scol );

    protected:
        void   initColor( SchemeColor &scol );

    protected:
        uint16_t                    color_bits;
        std::vector< SchemeColor >  colors;
};


#endif /// of __COLORSCHEME_H__
