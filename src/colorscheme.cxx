#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <tinyxml2.h>
#include "colorscheme.h"

using namespace tinyxml2;
using namespace std;

ColorScheme::ColorScheme( unsigned int bits, const char* itcfn )
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
        XMLElement* pListRoot = xdoc.FirstChildElement( "plist" );
        if ( pListRoot == nullptr )
            return retb;

        // find <dict> ...
        XMLElement* pDictRoot = pListRoot->FirstChildElement( "dict" );
        if ( pDictRoot != nullptr )
        {
            XMLElement* pKey = pDictRoot->FirstChildElement( "key" );
            XMLElement* pDict = pDictRoot->FirstChildElement( "dict" );

            while( pKey != NULL )
            {
                // sibling each <key> ... </key>
                SchemeColor newcol;
                initColor( newcol );
    
                const char* refKN = pKey->GetText();
                if ( refKN != NULL )
                    strncpy( newcol.name, refKN, 40 );

                if( pDict != NULL )
                {
                    XMLElement* pKA = NULL;
                    XMLElement* pKR = NULL;

                    size_t kroll = 0;

                    while( true )
                    {
                        if ( kroll == 0 )
                        {
                            pKA = pDict->FirstChildElement( "key" );
                            pKR = pDict->FirstChildElement( "real" );
                        }
                        else
                        {
                            if ( pKA != NULL )
                                pKA = pKA->NextSiblingElement( "key" );
                            if ( pKA != NULL )
                                pKR = pKA->NextSiblingElement( "real" );
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
                            if ( parseK == "Alpha Component" )
                            {
                                newcol.alpha = realFv;
                            }
                            else
                            if ( parseK == "Blue Component" )
                            {
                                newcol.blue = realFv;
                            }
                            else
                            if ( parseK == "Green Component" )
                            {
                                newcol.green = realFv;
                            }
                            else
                            if ( parseK == "Red Component" )
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
                    pKey  = pKey->NextSiblingElement( "key" );
                if ( pDict != NULL )
                    pDict = pDict->NextSiblingElement( "dict" );
            }
        }

        retb = true;
    }
    else
    {
        printf( "(debug)XML load failure = %s // ", itcfn );
    }

    return retb;
}

void ColorScheme::Unload()
{
    if ( colors.size() > 0 )
    {
        colors.clear();
    }
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
    memset( scol.name, 0, 40 );
    scol.alpha = 1.f;
    scol.red = 0.f;
    scol.green = 0.f;
    scol.blue = 0.f;
}

unsigned int ColorScheme::Scheme2RGBA8( SchemeColor &scol )
{
    unsigned int retcol_rgba = 0;

    float fcbits = (float)color_bits;

    unsigned char rv = (unsigned char)(scol.red * fcbits );
    unsigned char gv = (unsigned char)(scol.green * fcbits );
    unsigned char bv = (unsigned char)(scol.blue * fcbits );
    unsigned char av = (unsigned char)(scol.alpha * fcbits );

    retcol_rgba = ( rv << 24 ) | ( gv << 16 ) | ( bv << 8 ) | av;

    return retcol_rgba;
}
       
unsigned long long ColorScheme::Scheme2RGBA16( SchemeColor &scol )
{
    // 64bit length ...
    unsigned long long retcol_hdr = 0;

    float fcbits = (float)color_bits;

    unsigned short rv = (unsigned char)(scol.red * fcbits );
    unsigned short gv = (unsigned char)(scol.green * fcbits );
    unsigned short bv = (unsigned char)(scol.blue * fcbits );
    unsigned short av = (unsigned char)(scol.alpha * fcbits );

    // RED - GREEN - BLUE - ALPHA bits ( not padded ! )
    // 16  :  16   :   16 : 16   = 64 bits
    retcol_hdr = ( (unsigned long long)rv << 48 ) 
                 | ( (unsigned long long)gv << 32 ) 
                 | ( (unsigned long long)bv << 16 ) | av;

    return retcol_hdr;
}
