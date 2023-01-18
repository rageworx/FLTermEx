#ifndef __COLORSCHEME_H__
#define __COLORSCHEME_H__

#include <vector>

typedef struct _SchemeColor
{
    char    name[40];
    float   alpha;
    float   red;
    float   green;
    float   blue;
}SchemeColor;

class ColorScheme
{
    public:
        ColorScheme( unsigned int bits = 8, const char* itcfn = nullptr );
        ~ColorScheme();

    public:
        // notice - 
        // RGBA16 meaning HDR10 or HDR10+ not padded color order. 
        unsigned int Scheme2RGBA8( SchemeColor &scol );
        unsigned long long Scheme2RGBA16( SchemeColor &scol );

    public:
        bool Load( const char* itcfn = nullptr );
        void Unload();

    public:
        size_t findKey( const char* kn, bool exac = false );
        bool   getColor( size_t ki, SchemeColor &scol );
        bool   getColor( const char* kn, SchemeColor &scol );

    protected:
        void   initColor( SchemeColor &scol );

    protected:
        unsigned int                color_bits;
        std::vector< SchemeColor >  colors;
};


#endif /// of __COLORSCHEME_H__
