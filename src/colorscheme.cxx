#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <tinyxml2.h>
#include "colorscheme.h"

////////////////////////////////////////////////////////////////////////////////

using namespace tinyxml2;
using namespace std;

////////////////////////////////////////////////////////////////////////////////

#define CC_STR_PLIST_ROOT       "plist"
#define CC_STR_DICT             "dict"
#define CC_STR_KEY              "key"
#define CC_STR_REAL             "real"
#define CC_STR_COMPO_ALPHA      "Alpha Component"
#define CC_STR_COMPO_RED        "Red Component"
#define CC_STR_COMPO_GREEN      "Green Component"
#define CC_STR_COMPO_BLUE       "Blue Component"

////////////////////////////////////////////////////////////////////////////////

ColorScheme::ColorScheme( uint16_t bits, const char* itcfn )
  : color_bits( bits )
{
    // fix color space error : 8 to 16 
    if ( color_bits == 0 )
        color_bits = 8;
    else if ( color_bits > 16 )
        color_bits = 16;

    if ( itcfn != nullptr )
        Load( itcfn );
}

ColorScheme::~ColorScheme()
{
    Unload();
}

bool ColorScheme::Load( const char* itcfn )
{
    XMLDocument xdoc;
    bool retb = false;

    if( xdoc.LoadFile( itcfn ) == XML_SUCCESS )
    {
        // find <plist> ...
        XMLElement* pListRoot = xdoc.FirstChildElement( CC_STR_PLIST_ROOT );
        if ( pListRoot == nullptr )
            return retb;

        Unload();

        // find <dict> ...
        XMLElement* pDictRoot = pListRoot->FirstChildElement( CC_STR_DICT );
        if ( pDictRoot != nullptr )
        {
            XMLElement* pKey = pDictRoot->FirstChildElement( CC_STR_KEY );
            XMLElement* pDict = pDictRoot->FirstChildElement( CC_STR_DICT );

            while( pKey != NULL )
            {
                // sibling each <key> ... </key>
                SchemeColor newcol;
                initColor( newcol );
    
                const char* refKN = pKey->GetText();
                if ( refKN != NULL )
                    strncpy( newcol.name, refKN, COLORSCHEME_NAME_LEN );

                if( pDict != NULL )
                {
                    XMLElement* pKA = NULL;
                    XMLElement* pKR = NULL;

                    size_t kroll = 0;

                    while( true )
                    {
                        if ( kroll == 0 )
                        {
                            pKA = pDict->FirstChildElement( CC_STR_KEY );
                            pKR = pDict->FirstChildElement( CC_STR_REAL );
                        }
                        else
                        {
                            if ( pKA != NULL )
                                pKA = pKA->NextSiblingElement( CC_STR_KEY );
                            if ( pKA != NULL )
                                pKR = pKA->NextSiblingElement( CC_STR_REAL );
                        }

                        const char* refN = NULL;
                        const char* refR = NULL;

                        if ( pKA != NULL )
                            refN = pKA->GetText();
                        if ( pKR != NULL )
                            refR = pKR->GetText();

                        if ( ( refN != NULL ) && ( refR != NULL ) )
                        {
#ifdef DEBUG_PLIST_PARSE
                            printf( "(debug)%s::%s == %s\n",
                                    refKN,
                                    refN,
                                    refR );
                            fflush( stdout );
#endif /// of DEBUG_PLIST_PARSE
                            string parseK = refN;
                            float  realFv = atof( refR );
                            
                            if ( parseK == CC_STR_COMPO_ALPHA )
                            {
                                newcol.alpha = realFv;
                            }
                            else
                            if ( parseK == CC_STR_COMPO_BLUE )
                            {
                                newcol.blue = realFv;
                            }
                            else
                            if ( parseK == CC_STR_COMPO_GREEN )
                            {
                                newcol.green = realFv;
                            }
                            else
                            if ( parseK == CC_STR_COMPO_RED )
                            {
                                newcol.red = realFv;
                            }
                        }
                        else
                            break;
                        
                        kroll++;
                    }
                }

                colors.push_back( newcol );
#ifdef DEBUG
                printf( "[%s]::R(%f),G(%f),B(%f),A(%f):: %08X\n",
                        newcol.name,
                        newcol.red,
                        newcol.green,
                        newcol.blue,
                        newcol.alpha,
                        Scheme2RGBA8( newcol ) );
                fflush( stdout );
#endif
                // Step to next element --
                if ( pKey != NULL )
                    pKey  = pKey->NextSiblingElement( CC_STR_KEY );
                if ( pDict != NULL )
                    pDict = pDict->NextSiblingElement( CC_STR_DICT );
            }
        }

        if ( colors.size() > 0 )
            retb = true;
    }
#ifdef DEBUG
    else
    {
        printf( "(debug)XML load failure = %s // ", itcfn );
    }
#endif /// of DEBUG
    return retb;
}

void ColorScheme::Unload()
{
    if ( colors.size() > 0 )
    {
        colors.clear();
    }
}

size_t ColorScheme::size()
{
    return colors.size();
}

size_t ColorScheme::findKey( const char* kn, bool exac )
{
    if ( colors.size() > 0 )
    {
        for ( size_t cnt=0; cnt<colors.size(); cnt++ )
        {
            if ( strlen( colors[cnt].name  ) > 0 )
            {
                if ( exac == false )
                {
                    if ( strstr( colors[cnt].name, kn ) != NULL )
                        return cnt;
                }
                else
                {
                    if ( strcmp( colors[cnt].name, kn ) == 0 )
                        return cnt;
                }
            }
        }
    }

    return string::npos;
}

bool ColorScheme::getColor( size_t ki, SchemeColor &scol )
{
    if ( ki < colors.size() )
    {
        memcpy( &scol, &colors[ki], sizeof( SchemeColor ) );

        return true;;
    }
    return false;
}

bool ColorScheme::getColor( const char* kn, SchemeColor &scol )
{
    size_t ki = findKey( kn, true );
    if ( ki != string::npos )
    {
        return getColor( ki, scol );
    }

    return false;
}

void ColorScheme::initColor( SchemeColor &scol )
{
    memset( scol.name, 0, COLORSCHEME_NAME_LEN );
    scol.alpha = 1.f;
    scol.red = 0.f;
    scol.green = 0.f;
    scol.blue = 0.f;
}

unsigned int ColorScheme::Scheme2RGBA8( SchemeColor &scol )
{
    uint32_t retcol_rgba = 0;

    float fcbits = (float)color_bits;

    uint8_t rv = (uint8_t)(scol.red * fcbits );
    uint8_t gv = (uint8_t)(scol.green * fcbits );
    uint8_t bv = (uint8_t)(scol.blue * fcbits );
    uint8_t av = (uint8_t)(scol.alpha * fcbits );

    retcol_rgba = ( rv << 24 ) | ( gv << 16 ) | ( bv << 8 ) | av;

    return retcol_rgba;
}
       
unsigned long long ColorScheme::Scheme2RGBA16( SchemeColor &scol )
{
    // 64bit length ...
    uint64_t retcol_hdr = 0;

    float fcbits = (float)color_bits;

    uint16_t rv = (uint8_t)(scol.red * fcbits );
    uint16_t gv = (uint8_t)(scol.green * fcbits );
    uint16_t bv = (uint8_t)(scol.blue * fcbits );
    uint16_t av = (uint8_t)(scol.alpha * fcbits );

    // RED - GREEN - BLUE - ALPHA bits ( not padded ! )
    // 16  :  16   :   16 : 16   = 64 bits
    retcol_hdr = ( (uint64_t)rv << 48 ) 
                 | ( (uint64_t)gv << 32 ) 
                 | ( (uint64_t)bv << 16 ) | av;

    return retcol_hdr;
}
