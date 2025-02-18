//
// FLTermEx -- FLTK based terminal emulator
//
//    example application using the Fl_Term widget.
//
// Copyright 2017-2021 by Yongchao Fan.
// Copyright 2022-2023 Raphael Kim.
//
// This library is free software distributed under GNU GPL 3.0,
// see the license at:
//
//     https://github.com/rageworx/FLTermEx/blob/master/LICENSE
//
// Please report all bugs and problems on the following page:
//
//     https://github.com/rageworx/FLTermEx/issues/new
//

// This source code is not for MSVC -
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <thread>

#include "host.h"
#include "ssh2.h"
#include "Fl_Term.h"
#include "Fl_Browser_Input.h"

// prevent to macOS11 OpenGL deprecation warnings - 
// macOS13 still supports OpenGL as well for now .
#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <FL/platform.H>
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Preferences.H>

#include "resource.h"

#define TABHEIGHT   16
#ifdef __APPLE__
  #define MENUHEIGHT 0
  #define DEFAULTFONT "Menlo Regular"
#else
  #define MENUHEIGHT TABHEIGHT
  #ifdef _WIN32
    #define DEFAULTFONT "Consolas"
  #else
    #define DEFAULTFONT "Courier New"
  #endif
#endif

#define DEFAULT_APP_TITLE	"FLTermEx"
#define DEFAULTFONTSIZE	    12
#define DEFAULTROWS			25
#define DEFAULTCOLUMNS		80

#ifndef _MAX_PATH
	#define _MAX_PATH		512
#endif

const char ABOUT_TERM2[]="\r\n\n\
\tFLTermEx is a simple, small, scriptable terminal emulator,\r\n\n\
\ta serial/telnet/ssh/sftp/netconf client that features:\r\n\n\n\
\t    * light-weight minimalist design w/ FLTK\r\n\n\
\t    * multi-platform open source\r\n\n\
\t    * command history and autocompletion\r\n\n\
\t    * drag&drop text to run commands in batch\r\n\n\
\t    * drag&drop files to transfer to remote host\r\n\n\
\t    * scripting interface at xmlhttp://127.0.0.1:%d\r\n\n\n\
\tVersion %s ©2022-2023 Raphael, Kim, 2018-2021 Yongchao Fan\r\n\n\
\thttps://github.com/rageworx/FLTermEx\r\n\n";
const char FLTERM[]="\r\033[32mFLTermEx > \033[37m";

int httport;
void httpd_init();
void httpd_exit();

Fl_Window* 			pWindow = nullptr;
Fl_Tabs*			pTabs = nullptr;
Fl_Term*			pTerm = nullptr;
Fl_Browser_Input*	pCmd = nullptr;
Fl_Sys_Menu_Bar*	pMenuBar = nullptr;
Fl_Font 			fontnum = FL_COURIER;

static char fontname[256] = DEFAULTFONT;
static int fontsize = DEFAULTFONTSIZE;
static int termcols = DEFAULTCOLUMNS;
static int termrows = DEFAULTROWS;
static bool sendtoall = false;
static bool local_edit = false;
static double opacity = 1.0;

#if defined (__APPLE__)
void setTransparency(Fl_Window *pWin, double alpha);//cocoa_wrapper.mm
#elif defined (_WIN32)
void setTransparency(Fl_Window *pWin, double alpha)
{
    HWND hwnd = fl_xid(pWin);
    LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (!(exstyle & WS_EX_LAYERED)) {
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);
    }
    SetLayeredWindowAttributes(hwnd, 0, alpha*255, LWA_ALPHA);  
}
#elif defined (__linux__) || defined (__unix__)
void setTransparency(Fl_Window *pWin, double alpha)
{
    Atom atom = XInternAtom(fl_display, "_NET_WM_WINDOW_OPACITY", False);
    uint32_t opacity = alpha*0xFFFFFFFF;
    XChangeProperty(fl_display, fl_xid(pWin), atom, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&opacity, 1);
}
#endif

#define OPEN_FILE       Fl_Native_File_Chooser::BROWSE_FILE
#define SAVE_FILE       Fl_Native_File_Chooser::BROWSE_SAVE_FILE

#define DEFAULT_COLOR           0x30303000
#define DEFAULT_LABELCOLOR      0xC0C0C000

static Fl_Native_File_Chooser fnfc;

const char* file_chooser(const char *title, const char *filter, int type)
{
    fnfc.title(title);
    fnfc.filter(filter);
    fnfc.type(type);
    if ( fnfc.show()!=0 ) return NULL;
    return fnfc.filename();
}

void about_cb(Fl_Widget *w, void *data)
{
    char buf[4096] = {0};
    snprintf(buf, 4096, ABOUT_TERM2, httport, APP_VERSION_STR );
    pTerm->disp(buf);
}

const char *kb_gets(const char *prompt, int echo)
{
    if ( !pTerm->live() ) return NULL;
    return pTerm->gets(prompt, echo);
}

void resize_window(int cols, int rows)
{
    fl_font(fontnum, fontsize);
    float font_width = fl_width("abcdefghij")/10;
    int font_height = fl_height();
    int w = cols*font_width;
    int h = rows*font_height+font_height/2;
    if ( pTabs!=NULL ) h+=TABHEIGHT;
    pWindow->resize(pWindow->x(), pWindow->y(), w, h+MENUHEIGHT);
}

static bool title_changed = false;

void term_cb(Fl_Widget *w, void *data ) //called when term connection changes
{
    Fl_Term *term=(Fl_Term *)w;
    if ( term==pTerm ) {
        title_changed = true;
    }
    if ( data==NULL ) {//disconnected
        if ( pCmd->visible() ) term->disp(FLTERM);
    }
    if ( pTabs!=NULL ) pTabs->redraw();
}
/*******************************************************************************
*  tab management functions                                                    *
*******************************************************************************/
void tab_act(Fl_Term *pt)
{
    char label[64];
    if ( pTerm!=NULL ) {    //remove "x" from previous active tab
        strcpy(label, pTerm->label());
        char *p = strstr(label, " @-31+");
        if ( p!=NULL ) {
            *p=0;
            pTerm->copy_label(label);
        }
    }

    pTabs->value(pTerm=pt);
    pTerm->take_focus();
    pTerm->textfont(fontnum);
    pTerm->textsize(fontsize);

    strcpy(label, pTerm->label());  //add "x" to current active tab
    strcat(label, " @-31+");

    pTerm->copy_label(label);
    
    title_changed = true;
}

void tab_cb(Fl_Widget *w)
{
    if ( pTabs->value()==pTerm ) 
	{      //clicking on active tab
        if ( pTerm->live() )
		{   //confirm before disconnect
            if ( fl_choice("Disconnect from %s?", "Yes", "No", 
                            0, pTerm->label())==1 ) 
				return;
		}

        pTerm->disconn();
        pTerm->clear();

        if ( pTabs->children()>1 ) 
		{    //delete if there is more than one
            pTabs->remove(pTerm);
            pTabs->value(pTabs->child(0));
            pTerm = NULL;
        }
    }

    tab_act((Fl_Term *)pTabs->value()); //activate new tab
    pTabs->redraw(); 
}

void tab_new()
{
    if ( pTabs==NULL ) 
	{
        pTabs = new Fl_Tabs(0, MENUHEIGHT, pWindow->w(),
                                pWindow->h()-MENUHEIGHT);
        pTabs->when(FL_WHEN_RELEASE|FL_WHEN_CHANGED|FL_WHEN_NOT_CHANGED);
        pTabs->callback(tab_cb);
        pTabs->selection_color(FL_CYAN);
        pTabs->end();
        pTerm->resize(0, MENUHEIGHT+TABHEIGHT, pTabs->w(),
                                    pTabs->h()-TABHEIGHT);
        pTabs->add(pTerm);
        pTabs->resizable(pTerm);
        pWindow->insert(*pTabs, 0);
        pWindow->resizable(pTabs);
    }

    Fl_Term *pt = new Fl_Term(0, 0, 800, 480, "term");
    pt->labelsize(fontsize);
    pt->textsize(fontsize);
    pt->callback(term_cb);
    pTabs->add(pt);
    tab_act(pt);
    pt->resize(0, MENUHEIGHT+TABHEIGHT, pTabs->w(), pTabs->h()-TABHEIGHT);
}

HOST *host_new(const char *hostname)
{
    HOST *host=NULL;
    if ( strncmp(hostname, "ssh " , 4)==0 ) 
	{
        host = new sshHost(hostname+4);
    }
    else 
	if ( strncmp(hostname, "sftp ", 5)==0 ) 
	{
        host = new sftpHost(hostname+5);
    }
    else 
	if ( strncmp(hostname, "telnet ", 7)==0 ) 
	{
        host = new tcpHost(hostname+7);
    }
    else 
	if ( strncmp(hostname, "serial ", 7)==0 ) 
	{
        host = new comHost(hostname+7);
    }
    else 
	if ( strncmp(hostname, "netconf ", 8)==0 ) 
	{
        char netconf[256] = {0};
        strcpy(netconf, "-s ");
        strcat(netconf, hostname);

        host = new sshHost(netconf);
    }
    else
    {
		host = new pipeHost(hostname);
	}

    return host;
}

void term_connect(const char *hostname)
{
    if ( pTerm->live() ) 
		tab_new();

    HOST *host=host_new(hostname);
    
	if ( host!=NULL  ) 
	{
        pTerm->connect(host, NULL);
        char label[64];
        strcpy(label, pTerm->label());
        strcat(label, " @-31+");
        pTerm->copy_label(label);
    }
}

bool tab_match(int i, char *label)
{
    Fl_Term *t = (Fl_Term *)pTabs->child(i);
    if ( strncmp(t->label(), label, strlen(label))==0 ) 
	{
        tab_act(t);
        return true;
    }

    return false;
}

int term_command(char *cmd, const char **preply)
{
    int rc = 0;
    if ( strncmp(cmd, "!Tab", 4)==0 ) 
	{
        if ( cmd[4]==' ' && pTabs!=NULL ) 
		{
            int i, c = pTabs->find(pTerm);

            for ( i=c+1; i<pTabs->children(); i++ ) 
			{   //search forward
                if ( tab_match(i, cmd+5) )break;
            }            

			//wrap around
            if ( i==pTabs->children() ) for ( i=0; i<c; i++ ) 
			{
                if ( tab_match(i, cmd+5) ) break;
            }
        }
        else
            tab_new();

        if ( preply!=NULL ) 
		{
            *preply = pTerm->label();
            rc = strlen(*preply);
        }
    }
    else 
	{
        rc = pTerm->command(cmd, preply);
    
	}

    return rc;
}

/*******************************************************************************
*  connection dialog functions                                                 *
*******************************************************************************/
Fl_Window*			pConnectDlg = nullptr;
Fl_Choice*			pProtocol = nullptr;
Fl_Input_Choice*	pPort = nullptr;
Fl_Input_Choice*	pHostname = nullptr;
Fl_Button*			pConnect = nullptr;
Fl_Button*			pCancel = nullptr;

const char *ports[] = {
#if defined(_WIN32)
                    "ipconfig", "COM1",
#elif defined(__APPLE__)
                    "/bin/zsh", "tty.usbserial",
#else //Linux
                    "/bin/bash", "ttyS0",
#endif
                    "23", "22", "22", "830"
};
                    
const char *bauds[]={
	"9600,n,8,1", 
	"14400,n,8,1", 
	"19200,n,8,1", 
	"38400,n,8,1",
    "57600,n,8,1", 
	"115200,n,8,1", 
	"150000,n,8,1",
    "230400,n,8,1",
	"256000,n,8,1",
	""
};
                    
void protocol_cb(Fl_Widget *w)
{
    static int proto = 3;

    if ( proto==0 || proto==1 ) 
    {
        pHostname->menubutton()->clear();
        pHostname->value("192.168.1.1");
    }

    proto = pProtocol->value();
    pPort->value(ports[proto]);
    pPort->label(proto==0?"Shell:":" Port:");
    if ( proto==1 ) 
    {
        pHostname->menubutton()->clear();
        pHostname->label("Settings:");

		size_t idx = 0;
        while( true )
        {
			if( strlen( bauds[idx] ) > 0 )
            	pHostname->add(bauds[idx]);
			else
				break;
			idx++;
        }

		if( idx > 0 )
        	pHostname->value( idx / 2 );

#ifdef _WIN32
        // need to improved with Windows API enumeration -
        // will be replaced -
        char port[32]="\\\\.\\";
        for ( size_t i=1; i<32; i++ ) { //auto detect serial ports
            snprintf(port+4, 32, "COM%zu", i);
            HANDLE hPort = CreateFileA(port, GENERIC_READ, 0, NULL,
                                            OPEN_EXISTING, 0, NULL);
            if ( hPort!=INVALID_HANDLE_VALUE ) {
                CloseHandle(hPort);
                pPort->add(port+4);
                pPort->value(port+4);
            }
        }
#endif
    }
    else 
    {
        pHostname->label("      Host:");
        if ( proto==0 ) pHostname->value("");
    }
}

void menu_host_cb(Fl_Widget *w, void *data)
{
    term_connect(pMenuBar->text());
}

void connect_cb(Fl_Widget *w)
{
    char buf[256] = "!";

    int proto = pProtocol->value();

    if ( proto==0 ) 
    {       //local shell
        strcat(buf, pPort->value());
    }
    else 
    {
        strcat(buf, pProtocol->mvalue()->label());
        strcat(buf, " ");
        if ( proto==1 ) 
		{   //serial connection
            strcat(buf, pPort->value());
            strcat(buf, ":");
            strcat(buf, pHostname->value());
        }
        else 
        {   //telnet/ssh/sftp/netconf
            pHostname->add(pHostname->value());
            strcat(buf, pHostname->value());
            if ( strcmp(ports[proto],pPort->value())!=0 ) {
                strcat(buf, ":");
                strcat(buf, pPort->value());
            }
        }
    }
    
    pConnectDlg->hide();

    if ( pCmd->add(buf)!=0 )
    {
        pMenuBar->insert(pMenuBar->find_index("Script")-1, 
                            buf+1, 0, menu_host_cb);
    }
    
    term_connect(buf+1);
}

void cancel_cb(Fl_Widget *w)
{
    w->parent()->hide();
}

void connect_dlg_build()
{
    pConnectDlg = new Fl_Window(360, 200, "Connect");
    {
        pConnectDlg->color(DEFAULT_COLOR);
        pConnectDlg->labelcolor(DEFAULT_LABELCOLOR);

        pProtocol = new Fl_Choice(100,20,192,24, "Protocol:");
        pProtocol->color( fl_darker( pConnectDlg->color() ) );
        pProtocol->labelcolor( pConnectDlg->labelcolor() );
        pProtocol->textcolor( pConnectDlg->labelcolor() );

        pPort = new Fl_Input_Choice(100,60,192,24, "Port:");
        pPort->color( fl_darker( pConnectDlg->color() ) );
        pPort->labelcolor( pConnectDlg->labelcolor() );

        pHostname = new Fl_Input_Choice(100,100,192,24,"Host:");
        pHostname->color( fl_darker( pConnectDlg->color() ) );
        pHostname->labelcolor( pConnectDlg->labelcolor() );

        pConnect = new Fl_Button(200,160,80,24, "Connect");
        pConnect->color( fl_darker( pConnectDlg->color() ) );
        pConnect->labelcolor( pConnectDlg->labelcolor() );

        pCancel = new Fl_Button(80,160,80,24, "Cancel");
        pCancel->color( fl_darker( pConnectDlg->color() ) );
        pCancel->labelcolor( pConnectDlg->labelcolor() );

        pProtocol->textsize(fontsize); 
        pProtocol->labelsize(fontsize);
        pHostname->textsize(fontsize); 
        pHostname->labelsize(fontsize);
        pPort->textsize(fontsize); 
        pPort->labelsize(fontsize);
        pConnect->labelsize(fontsize);
        pConnect->shortcut(FL_Enter);
        pCancel->labelsize(fontsize);
        pProtocol->add("shell|serial|telnet|ssh|sftp|netconf");
        pProtocol->value(3);
        pPort->value("22");
        pHostname->add("192.168.1.1");
        pHostname->value("192.168.1.1");

        pProtocol->callback(protocol_cb);
        pConnect->callback(connect_cb);
        pCancel->callback(cancel_cb);
    }

    pConnectDlg->end();
    pConnectDlg->set_modal();
}

void connect_dlg(Fl_Widget *w, void *data)
{
    pConnectDlg->resize(pWindow->x()+100, pWindow->y()+150, 360, 200);
    pConnectDlg->show();
    pHostname->take_focus();
}

/*******************************************************************************
* font dialog functions                                                        *
*******************************************************************************/
Fl_Window *pFontDlg;
Fl_Hold_Browser *fontobj, *sizeobj;
Fl_Button *donebtn;
static int **sizes;
static int *numsizes;

void font_cb(Fl_Widget *, long)
{
    int sel = fontobj->value();
    if (!sel) return;
    /* casting error fix -- */
    void* pcf = fontobj->data(sel);
    fontnum = *(Fl_Font*)&pcf; 
    /* -------------------- */
    pTerm->textfont(fontnum);
    pCmd->textfont(fontnum);
    resize_window(pTerm->sizeX(), pTerm->sizeY());
    sizeobj->clear();
    int n = numsizes[fontnum];
    int *s = sizes[fontnum];
    if ( ( n > 0 ) && (s[0] == 0) )
	{	// many sizes;
        int j = 1;
        for (int i=6; i<=32 || i<s[n-1]; i++) 
		{
            char buf[20] = {0};
            snprintf(buf,20,"%d",i);
            sizeobj->add(buf);
            if ( i==fontsize ) sizeobj->value(sizeobj->size());
        }
    } 
    else 
	{	// some sizes
        int w = 0;
        for (int i = 0; i < n; i++) 
		{
            char buf[20] = {0};
            snprintf(buf,20,"@b%d",s[i]);
            sizeobj->add(buf);
            if ( s[i]==fontsize ) sizeobj->value(sizeobj->size());
        }
    }

	int t = 0;
	const char* fface = Fl::get_font_name(fontnum, &t);
	if ( fface != nullptr )
	{
		if ( strcmp( fontname, fface ) != 0 )
			strncpy( fontname, fface, 256 );
	}
}

void size_cb(Fl_Widget *, long) 
{
    int i = sizeobj->value();
    if (!i) return;
    const char *c = sizeobj->text(i);
    while (*c < '0' || *c > '9') c++;
    fontsize = atoi(c);
    pTerm->textsize(fontsize);
    pCmd->textsize(fontsize);
    resize_window(pTerm->sizeX(), pTerm->sizeY());
}

void font_dlg_build()
{
    pFontDlg = new Fl_Double_Window(400, 240, "Select a font");
    {
        pFontDlg->color(DEFAULT_COLOR);
        pFontDlg->labelcolor(DEFAULT_LABELCOLOR);
        fontobj = new Fl_Hold_Browser(10, 20, 290, 210, "Face:");
        fontobj->format_char(0);
        fontobj->align(FL_ALIGN_TOP|FL_ALIGN_LEFT);
        fontobj->box(FL_FRAME_BOX);
        fontobj->color( fl_darker( DEFAULT_COLOR ) );
        fontobj->labelcolor(DEFAULT_LABELCOLOR);
        fontobj->textcolor(DEFAULT_LABELCOLOR);
        fontobj->callback(font_cb);
        sizeobj = new Fl_Hold_Browser(310, 20, 80, 180, "Size:");
        sizeobj->align(FL_ALIGN_TOP|FL_ALIGN_LEFT);
        sizeobj->box(FL_FRAME_BOX);
        sizeobj->color( fl_darker( DEFAULT_COLOR ) );
        sizeobj->labelcolor(DEFAULT_LABELCOLOR);
        sizeobj->textcolor(DEFAULT_LABELCOLOR);
        sizeobj->callback(size_cb);
        donebtn = new Fl_Button(310, 206, 80, 24, "Done");
        donebtn->color( fl_darker( DEFAULT_COLOR ) );
        donebtn->labelcolor(DEFAULT_LABELCOLOR);
        donebtn->callback(cancel_cb);
    }

    pFontDlg->end();
    pFontDlg->set_modal();

    int fi = 0;

    size_t k = Fl::set_fonts( fi ? ( fi>0 ? "*" : 0) : "-*" );
    
	sizes = new int*[k];
    numsizes = new int[k];

    for (size_t i = 0; i < k; i++) 
    {
        int t; 
        const char *name = Fl::get_font_name((Fl_Font)i,&t);
        if ( name != NULL )
        {
            if ( ( t == 0 ) && ( name[0] != '@' ) )
            {
#ifdef DEBUG
                printf( "font insert = %s [%d]\n", name, t );
                fflush( stdout );
#endif
                fontobj->add(name, (void *)i);
                if ( strcmp(fontname, name)==0 ) 
                { 
                    fontobj->select(fontobj->size());
                    fontnum = i;
                }

                int *s;
                int n = Fl::get_font_sizes((Fl_Font)i, s);
                numsizes[i] = n;
                if ( n ) 
                {
                    sizes[i] = new int[n];

                    for (int j=0; j<n; j++) 
                    {
                        sizes[i][j] = s[j];
                    }
                }
            }
        }
    }
}

void font_dlg(Fl_Widget *w, void *data)
{
    pFontDlg->resize(pWindow->x()+200, pWindow->y()+100, 400, 240);
    pFontDlg->show();
    font_cb(NULL, 0);
    fontobj->take_focus();
}

/*******************************************************************************
* scripting functions                                                          *
*******************************************************************************/
Fl_Window *pScriptDlg;
Fl_Button *quitBtn, *pauseBtn;

void quit_cb(Fl_Widget *w) 
{
    pTerm->quit_script();
    w->parent()->hide();
}

void pause_cb(Fl_Widget *w)
{
    if ( pTerm->script_running() )
        pauseBtn->label(pTerm->pause_script()?"Resume":"Pause");
    else
        w->parent()->hide();
}

void script_dlg_build()
{
    pScriptDlg = new Fl_Double_Window(220, 72, "Script Control");
    {
        pScriptDlg->color(DEFAULT_COLOR);
        pScriptDlg->labelcolor(DEFAULT_LABELCOLOR);
        pauseBtn = new Fl_Button(20, 20, 80, 32, "Pause");
        pauseBtn->color( fl_darker( pScriptDlg->color() ) );
        pauseBtn->labelcolor( pScriptDlg->labelcolor() );
        pauseBtn->callback(pause_cb);
        pauseBtn->labelsize(fontsize);
        quitBtn = new Fl_Button(120, 20, 80, 32, "Quit");
        quitBtn->color( fl_darker( pScriptDlg->color() ) );
        quitBtn->labelcolor( pScriptDlg->labelcolor() );
        quitBtn->callback(quit_cb);
        quitBtn->labelsize(fontsize);
    }
    pScriptDlg->end();
    pScriptDlg->set_modal();
    pScriptDlg->resize(pWindow->x()+pWindow->w()-220, 
                            pWindow->y()+40, 220, 72);
}

void script_open( const char *fn )
{
    pTerm->learn_prompt();
    const char *ext = fl_filename_ext(fn);
    if ( ext!=NULL ) {
        if ( strcmp(ext, ".html")==0 ) {
            char url[1024], msg[1024];
            snprintf(url, 1024, "http://127.0.0.1:%d/%s", httport, fn);
            if ( !fl_open_uri(url, msg, 1024) ) fl_alert("Error:%s",msg);
            return;
        }
    }
#ifdef _WIN32
    char http_port[16] = {0};
    snprintf(http_port, 16, "%d", httport);
    ShellExecuteA(NULL, "open", fn, http_port, NULL, SW_SHOW);
#else
    char cmd[4096] = "open ";
    strcat(cmd, fn);
    system(cmd);
#endif
}

void script_cb(Fl_Widget *w, void *data)
{
    const Fl_Menu_Item *menu = pMenuBar->menu();
    const Fl_Menu_Item script = menu[pMenuBar->value()];
    script_open((char *)script.user_data());
}

/*******************************************************************************
* command editor functions                                                     *
*******************************************************************************/
void cmd_send(Fl_Term *t, const char *cmd)
{
    if ( *cmd=='!' ) 
	{
        if ( strncmp(cmd, "!scp ", 5)==0 || strncmp(cmd, "!tun", 4)==0 ) 
            pTerm->run_script(cmd);
        else 
		if ( strncmp(cmd, "!script ", 8)==0 )
            script_open(cmd+8);
        else
            t->command(cmd, NULL);
    }
    else 
	{
        if ( t->live() ) 
		{
            t->send(cmd);
            t->send("\r");
        }
        else 
		{
             if ( *cmd )
                term_connect(cmd);  //new connection
            else
                pTerm->send("\r");  //restart connection
        }
    }
}

void cmd_cb(Fl_Widget *o, void *p)
{
    if ( p!=NULL ) {                        //from do_callback
        pTerm->send((const char *)p);
    }
    else 
	{
        const char *cmd = pCmd->value();    //normal callback
        pCmd->add(cmd);
        if ( !sendtoall || pTabs==NULL ) 
		{
            cmd_send(pTerm, cmd);
        }
        else 
		{
            for ( int i=0; i<pTabs->children(); i++ )
                cmd_send((Fl_Term *)pTabs->child(i), cmd);
        }
        pCmd->value("");
    }
}

void localedit(bool e)
{
    local_edit = e;
    Fl_Menu_Item * pItem = (Fl_Menu_Item *)
                            pMenuBar->find_item("Options/Local &Edit");
    if ( local_edit ) 
	{
        if ( !pTerm->live() ) pTerm->disp(FLTERM);
        pCmd->show();
        pTerm->redraw();
        pItem->set();
    }
    else 
	{
        pCmd->hide();
        pTerm->take_focus();
        pItem->clear();
    }
}
bool show_editor(int x, int y, int w, int h)
{
    if ( local_edit && x>=0 ) 
	{
        pCmd->show();
        pCmd->take_focus();
        pCmd->resize(x, y, w, h);
        return true;
    }
    else 
	if ( pCmd->visible() ) 
	{
        pCmd->hide();
        pTerm->take_focus();
    }

    return false;
}
/*******************************************************************************
* menu call back functions                                                     *
*******************************************************************************/
void menu_cb(Fl_Widget *w, void *data)
{
    const char *menutext = pMenuBar->text();
    if ( strcmp(menutext, "&Disconnect")==0 ) 
	{
        pTerm->disconn();
    }
    else 
	if ( strncmp(menutext, "&Log", 4)==0 ) 
	{
        const char *fname = pTerm->logg();
        if ( fname==NULL ) 
		{
            fname = file_chooser("log session to file:",
                                "Log\t*.log", SAVE_FILE);
            if ( fname==NULL ) return;
        }
        else 
		{
            if ( fl_choice("Stop logging to %s?", "No", "Yes", 0, fname)==0 )
                 return;
        }
        pTerm->logg(fname);
    }
    else 
	if ( strcmp(menutext, "&Save...")==0 ) 
	{
        const char *fname = file_chooser("save buffer to file:", 
                                         "Text\t*.txt", SAVE_FILE);
        if ( fname!=NULL ) pTerm->save(fname);
    }
    else 
	if ( strcmp(menutext, "Search...")==0 ) 
	{
        const char *keyword="";
        while ( true ) 
		{
            keyword = fl_input("Search buffer for:", keyword);
            if ( keyword==NULL ) break;
            pTerm->srch(keyword);
        }
    }
    else 
	if ( strcmp(menutext, "&Run...")==0 ) 
	{
        const char *fname = file_chooser("script:", "All\t*.*", OPEN_FILE);
        if ( fname!=NULL ) 
		{
            script_open(fname);
            pMenuBar->insert(pMenuBar->find_index("Options")-1,
                        fl_filename_name(fname), 0, script_cb, strdup(fname));
            char cmd[256]="!script ";
            strncpy(cmd+8, fname, 248);
            cmd[255] = 0;
            pCmd->add(cmd);
        }
    }
    else 
	if ( strcmp(menutext, "Local &Edit")==0 ) 
	{
        localedit(!local_edit);
    }
    else 
	if ( strcmp(menutext, "Send to All")==0 ) 
	{
        sendtoall = !sendtoall;
        pCmd->color(sendtoall?0x40004000:FL_BLACK);
    }
    else 
	if ( strcmp(menutext, "Transparency")==0 ) 
	{
        opacity =  (opacity==1.0) ? 0.875 : 1.0;
        setTransparency(pWindow, opacity);
    }
}

void close_cb(Fl_Widget *w, void *data)
{
    if ( pTabs==NULL ) 
	{    //not multi-tab
        if ( pTerm->live() ) 
		{
            if ( fl_choice("Disconnect and exit?", "No", "Yes", 0)==0 ) return;
            pTerm->disconn();
        }
    }
    else 
	{   //multi-tabbed
        bool confirmed = false;
        for ( int i=0; i<pTabs->children(); i++ ) 
		{
            Fl_Term *pt = (Fl_Term *)pTabs->child(i);
            if ( pt->live() ) 
			{
                if ( !confirmed ) 
				{
                    if (fl_choice("Disconnect all and exit?",
                                    "No", "Yes", 0)==0 ) return;
                    confirmed = true;
                } 

                pt->disconn();
            }
        }
    }
    pCmd->close();
    pWindow->hide();
}

#ifdef __APPLE__
	#define FL_CMD FL_META
#else
	#define FL_CMD FL_ALT
#endif

Fl_Menu_Item menubar[] = {
	{"Term",        FL_CMD+'t', 0,      0,  FL_SUBMENU},
	{"&Connect...", FL_CMD+'c', connect_dlg},
	{"&Log...",         0,      menu_cb},
	{"&Save...",        0,      menu_cb},
	{"Search...",       0,      menu_cb},
	{"&Disconnect", FL_CMD+'d', menu_cb,0,  FL_MENU_DIVIDER},
	{0},
	{"Script",      FL_CMD+'s', 0,      0,  FL_SUBMENU},
	{"&Run...",         0,      menu_cb,0,  FL_MENU_DIVIDER},
	{0},
	{"Options",     FL_CMD+'o', 0,      0,  FL_SUBMENU},
	{"&Font...",        0,      font_dlg},
	{"Local &Edit", FL_CMD+'e', menu_cb,0,  FL_MENU_TOGGLE},
	{"Send to All",     0,      menu_cb,0,  FL_MENU_TOGGLE},
	{"Transparency",    0,      menu_cb,0,  FL_MENU_TOGGLE},
#ifndef __APPLE__
	{"&About FLTerm",0,         about_cb},
#endif
	{0},
	{0}
};

/*******************************************************************************
*  dictionary functions, load at start, save at end of program                 *
*******************************************************************************/
// load_dict set working directory and load scripts to menu
#ifdef _WIN32
	const char *HOMEDIR = "USERPROFILE";
#else
	const char *HOMEDIR = "HOME";
#endif

const char *DICTFILE = ".FLTerm";

// Improvement issue:
// load_dict() need to use Fl_Preference.
#if 1
void load_dict()
{
	Fl_Preferences cfg( Fl_Preferences::USER_L, 
	                    DEFAULT_APP_TITLE,
						DEFAULT_APP_TITLE );

	cfg.get( "FontFace", (char*)fontname , DEFAULTFONT, 80 );
	cfg.get( "FontSize", fontsize, DEFAULTFONTSIZE );
	cfg.get( "TermSize.Columm", termcols, DEFAULTCOLUMNS );
	cfg.get( "TermSize.Row", termrows, DEFAULTROWS );
	cfg.get( "WindowOpacity", opacity, 1.f );

	if ( pWindow != nullptr )
	{
		int winL = pWindow->x();
		int winT = pWindow->y();
		int winW = pWindow->w();
		int winH = pWindow->h();
		cfg.get( "WindowPosition.Left", winL, winL );
		cfg.get( "WindowPosition.Top", winT, winT );
		cfg.get( "WindowPosition.Width", winW, winW );
		cfg.get( "WindowPosition.Height", winH, winH );

		if ( pTerm != nullptr )
		{
			pWindow->position( winL, winT );
			resize_window( termcols, termrows );
		}
		else
		{
			pWindow->resize( winL, winT, winW, winH );
		}
	}

	int  conSize = 0;
	cfg.get( "Connections", conSize, 0 );
	if( conSize > 0 )
	{
		for ( int cnt=0; cnt<conSize; cnt++ )
		{
			char conType[40] = {0};
			char conValue[40] = {0};
			char rmap[40] = {0};
			snprintf( rmap, 40, "Connection[%d].Type", cnt );
			cfg.get( rmap, conType, NULL, 40 );
			snprintf( rmap, 40, "Conection[%d].Value", cnt );
			cfg.get( rmap, conValue, NULL, 40 );

			if ( strlen( conValue ) > 0 )
			{
				if ( strncmp( conType, "ssh ",   4)==0 ||
					strncmp( conType, "sftp ",  5)==0 ||
					strncmp( conType, "telnet ",7)==0 ||
					strncmp( conType, "serial ",7)==0 ||
					strncmp( conType, "netconf ",8)==0 ) 
				{
					pMenuBar->insert( pMenuBar->find_index("Script")-1,
									  conType, 0, menu_host_cb );
					pHostname->add( conValue );
				}
			}
		}

		pHostname->value( pHostname->menu()->size() - 1 );
	}

	int scpSize = 0;
	cfg.get( "Scripts", scpSize, 0 );
	if ( scpSize > 0 )
	{
		for ( int cnt=0; cnt<scpSize; cnt++ )
		{
			char scpValue[_MAX_PATH] = {0};
			char rmap[40] = {0};
			snprintf( rmap, 40, "Script[%d]", cnt );
			cfg.get( rmap, scpValue, NULL, _MAX_PATH );

			if ( strlen( scpValue ) > 0 )
			{
				pMenuBar->insert( pMenuBar->find_index("Options")-1,
								   fl_filename_name( scpValue ), 0,
								   script_cb, strdup( scpValue ) );
			}
		}
	}

	char bootfn[_MAX_PATH] = {0};
	cfg.get( "Boot", bootfn, 0, _MAX_PATH );
	if ( strlen( bootfn ) > 0 )
	{
		script_open( bootfn );
	}
}

void save_dict()
{
	Fl_Preferences cfg( Fl_Preferences::USER_L, 
	                    DEFAULT_APP_TITLE,
						DEFAULT_APP_TITLE );

	if ( pTerm != NULL )
	{
		termcols = pTerm->sizeX();
		termrows = pTerm->sizeY();
	}

	cfg.set( "FontFace", fontname );
	cfg.set( "FontSize", fontsize );
	cfg.set( "TermSize.Columm" , termcols );
	cfg.set( "TermSize.Row", termrows );
	cfg.set( "WindowOpacity", opacity );

	if ( pWindow != nullptr )
	{
		cfg.set( "WindowPosition.Left", pWindow->x() );
		cfg.set( "WindowPosition.Top", pWindow->y() );
		cfg.set( "WindowPosition.Width", pWindow->w() );
		cfg.set( "WindowPosition.Height", pWindow->h() );
	}
}
#else
void load_dict()
{
    FILE *fp = fopen(DICTFILE, "r");
    if ( fp==NULL ) 
    {   // current directory doesn't have .hist
        if ( fl_chdir(getenv(HOMEDIR))==0 ) 
        {
            fp = fopen(DICTFILE, "r");
            if ( fp==NULL ) 
            {
                fp = fopen(".tinyTerm/tinyTerm.hist", "r");
            }
        }
    }
    if ( fp!=NULL ) 
    {
        char line[256];
        while ( fgets(line, 256, fp)!=NULL ) 
        {
            line[strcspn(line, "\r\n")] = 0;
            if ( *line=='~' ) {
                if ( strncmp(line+1, "FontFace ", 9)==0 ) 
                {
                    strncpy(fontname, line+10, 255);
                    fontname[255]=0;
                }
                else if ( strncmp(line+1, "FontSize ", 9)==0 ) 
                {
                    fontsize = atoi(line+10);
                }
                else if ( strncmp(line+1, "TermSize ", 9)==0 ) 
                {
                    sscanf(line+10, "%dx%d", &termcols, &termrows);
                }
                else if ( strncmp(line+1, "WindowOpacity", 12)==0 ) 
                {
                    opacity = atof(line+14);
                    Fl_Menu_Item * pItem = (Fl_Menu_Item *)
                                    pMenuBar->find_item("Options/Transparency");
                    pItem->set();
                }
                else if ( strcmp(line+1, "LocalEdit")==0 ) 
				{
                    localedit(true);
                    Fl_Menu_Item * pItem = (Fl_Menu_Item *)
                                    pMenuBar->find_item("Options/Local &Edit");
                    pItem->set();
                }
            }
            else 
            {
                pCmd->add(line);
                if ( *line=='!' ) 
                {
                    if ( strncmp(line+1, "ssh ",   4)==0 ||
                         strncmp(line+1, "sftp ",  5)==0 ||
                         strncmp(line+1, "telnet ",7)==0 ||
                         strncmp(line+1, "serial ",7)==0 ||
                         strncmp(line+1, "netconf ",8)==0 ) 
					{
                        pMenuBar->insert(pMenuBar->find_index("Script")-1,
                                                line+1, 0, menu_host_cb);
                        pHostname->add(strchr(line+1, ' ')+1);
                    }
                    else 
					if (strncmp(line+1, "script ", 7)==0 ) 
                    {
                        pMenuBar->insert(pMenuBar->find_index("Options")-1,
                                            fl_filename_name(line+8), 0,
                                            script_cb, strdup(line+8));
                    }
                    else 
					if ( strncmp(line+1, "Boot ", 5)==0 ) 
                    {
                        script_open(line+6);
                    }
                }
            }
        }
        fclose(fp);
    }
}

void save_dict()
{
    FILE *fp = fopen(DICTFILE, "w");
    if ( fp!=NULL )
	{
        int t;
        
		fprintf(fp, "~TermSize %dx%d\n", pTerm->sizeX(), pTerm->sizeY());
        fprintf(fp, "~FontFace %s\n", Fl::get_font_name(fontnum, &t));
        fprintf(fp, "~FontSize %d\n", fontsize);

        if ( local_edit )
			fprintf(fp, "~LocalEdit\n");
        
		if ( opacity!=1.0 ) 
            fprintf(fp, "~WindowOpacity %.3f\n", opacity);

        const char *p = pCmd->first();
        
		while ( p!=NULL ) 
		{
            if ( *p!='~' ) fprintf(fp, "%s\n", p);
            p = pCmd->next();
        }

        fclose(fp);
    }
}
#endif /// of 1

void redraw_cb(void *)
{
    if ( pTerm->pending() ) 
	{
        pTerm->redraw();
        if ( pCmd->visible() ) pCmd->redraw();
    }

    Fl::repeat_timeout(0.02, redraw_cb);
}

static char title[256]="FLTermEx     ";

void title_cb(void *)
{
    if ( title_changed ) 
	{
        strncpy(title+12, pTerm->title(), 240);
        pWindow->label(title);
        title_changed = false;
        if ( pTerm->sizeX()!=termcols || pTerm->sizeY()!=termrows ) {
            termcols = pTerm->sizeX();
            termrows = pTerm->sizeY();
            resize_window(termcols, termrows);
        }
    }

    if ( pTerm->script_running() )
        pScriptDlg->show();
    else 
        pScriptDlg->hide();

    Fl::repeat_timeout(1.0, title_cb);
}

int main(int argc, char **argv)
{
    httpd_init();
    libssh2_init(0);
    
#ifdef FLTK_EXT_VERSION
    Fl::scheme("flat");
    
    fl_message_size_ = fontsize;
    fl_message_window_color_ = DEFAULT_COLOR;
    fl_message_label_color_ = DEFAULT_LABELCOLOR;
    fl_message_button_color_[0] = fl_darker( DEFAULT_COLOR );
    fl_message_button_color_[1] = fl_darker( DEFAULT_COLOR );
    fl_message_button_color_[2] = fl_darker( DEFAULT_COLOR );
    fl_message_button_label_color_[0] = DEFAULT_LABELCOLOR;
    fl_message_button_label_color_[1] = DEFAULT_LABELCOLOR;
    fl_message_button_label_color_[2] = DEFAULT_LABELCOLOR;    
#else
    Fl::scheme("gtk+");
#endif

	// Set unique window class name to DEFAULT_APP_TITLE
    Fl_Double_Window::default_xclass( DEFAULT_APP_TITLE );
    Fl::lock();

    pWindow = new Fl_Double_Window(800, 640, DEFAULT_APP_TITLE );
    {
        pWindow->color(DEFAULT_COLOR);
        pMenuBar=new Fl_Sys_Menu_Bar(0, 0, pWindow->w(), MENUHEIGHT);
        pMenuBar->color(DEFAULT_COLOR);
        pMenuBar->labelcolor(DEFAULT_LABELCOLOR);
        pMenuBar->window_menu_style(Fl_Sys_Menu_Bar::no_window_menu);
        pMenuBar->menu(menubar);
        if ( MENUHEIGHT > 8 )
        { 
            pMenuBar->textsize(MENUHEIGHT - 2);
        }
        pMenuBar->textcolor(DEFAULT_LABELCOLOR);
#ifndef __APPLE__
        pMenuBar->about(about_cb, NULL);
#endif /// of __APPLE__
        pTerm = new Fl_Term(0, MENUHEIGHT, pWindow->w(),
                                pWindow->h()-MENUHEIGHT, "term");
        pTerm->labelsize(fontsize);
        pTerm->callback( term_cb );
        pCmd = new Fl_Browser_Input( 0, pWindow->h()-1, 1, 1, "");
        pCmd->box(FL_FLAT_BOX);
        pCmd->color(FL_BLACK);
        pCmd->textcolor(FL_GREEN);
        pCmd->cursor_color(FL_WHITE);
        pCmd->when(FL_WHEN_ENTER_KEY_ALWAYS);
        pCmd->callback(cmd_cb);
        pCmd->hide();
    }
    pWindow->callback(close_cb);
    pWindow->resizable(pTerm);
    pWindow->end();
    pTabs = NULL;
#ifdef _WIN32
    pWindow->icon((char*)LoadIcon(fl_display, MAKEINTRESOURCE(101)));
#endif

    connect_dlg_build();
    load_dict();        //get fontface
    font_dlg_build();   //get fontnum
    pTerm->textfont(fontnum);
    pTerm->textsize(fontsize);
    pCmd->textfont(fontnum);
    pCmd->textsize(fontsize);
    resize_window(termcols, termrows);
    pWindow->show();
    setTransparency(pWindow, opacity);
    script_dlg_build();

    char cwd[4096] = {0};
    fl_getcwd(cwd, 4096);   //save cwd set by load_dict()

    Fl::add_timeout(0.02, redraw_cb);
    Fl::add_timeout(1.0, title_cb);
    if ( !local_edit ) connect_dlg(NULL, NULL);
    about_cb( NULL, NULL );
    Fl::run();

    fl_chdir(cwd);          //in case cwd was changed in sftp_lcd
    save_dict();
    libssh2_exit();
    httpd_exit();
    return 0;
}

/**********************************HTTPd**************************************/
const char HEADER[]="HTTP/1.1 200 OK\nServer: FLTerm\n\
Access-Control-Allow-Origin: *\nContent-Type: text/plain\n\
Cache-Control: no-cache\nContent-length: %d\n\n";

const char *RFC1123FMT="%a, %d %b %Y %H:%M:%S GMT";
const char *exts[]={".txt",
                    ".htm", ".html",
                    ".js",
                    ".jpg", ".jpeg",
                    ".png",
                    ".css"
                    };
const char *mime[]={"text/plain",
                    "text/html", "text/html",
                    "text/javascript",
                    "image/jpeg", "image/jpeg",
                    "image/png",
                    "text/css"
                    };

void httpFile(int s1, char *file)
{
    char reply[4096], timebuf[128];
    time_t now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    int len;
    struct stat sb;
    if ( stat( file, &sb ) ==-1 ) 
	{
        int snplen = 4096;

        len=snprintf(reply, snplen, "HTTP/1.1 404 not found\nDate: %s\n", timebuf);
		snplen -= len;

        if ( snplen > 0 ) 
		{
            len+=snprintf(reply+len, snplen,"Server: FLTerm\nConnection: close");
            snplen -= len;
        }

        if ( snplen > 0 ) 
		{
            len+=snprintf(reply+len, snplen,"Content-Type: text/html\nContent-Length: 14");
            snplen -= len;
        }

        if ( snplen > 0 ) 
		{
            len+=snprintf(reply+len, snplen,"\n\nfile not found");
            snplen -= len;
        }

        send(s1, reply, len, 0);
        return;
    }

    FILE *fp = fopen( file, "rb" );
    if ( fp!=NULL ) 
	{
        int snplen = 4096;
        len=snprintf(reply, snplen, "HTTP/1.1 200 Ok\nDate: %s\n", timebuf);
        snplen -= len;

        if ( snplen > 0 ) 
		{
            len+=snprintf(reply+len, snplen, "Server: FLTerm\nConnection: close");
            snplen -= len;
        }

        const char *filext=strrchr(file, '.');
        int i=0;

        if ( filext!=NULL ) 
		{
            for ( int j=0; j<8; j++ )
                if ( strcmp(filext, exts[j])==0 ) i=j;
        }

        if ( snplen > 0 ) 
		{
            len+=snprintf(reply+len, snplen, "Content-Type: %s\n", mime[i]);
            snplen -= len;
        }

        long filesize = sb.st_size;
        if ( snplen > 0 ) 
		{
            len+=snprintf(reply+len, snplen, "Content-Length: %ld\n", filesize);
            snplen -= len;
        }
        
        strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &sb.st_mtime));
        if ( snplen >0 ) 
		{
            len+=snprintf(reply+len, snplen, "Last-Modified: %s\n\n", timebuf);
            snplen -= len;
        }

        send(s1, reply, len, 0);

        while ( (len=fread(reply, 1, 4096, fp))>0 )
		{
            if ( send(s1, reply, len, 0)==-1 ) break;
		}

        fclose(fp);
    }
}

void httpd( int s0 )
{
    struct sockaddr_in cltaddr;
    socklen_t addrsize=sizeof(cltaddr);
    char buf[4096], *cmd;
    char* bufp = buf;
    const char *reply;
    int cmdlen, replen, http_s1;

    while ( (http_s1=accept(s0,(struct sockaddr*)&cltaddr,&addrsize ))!=-1 ) 
	{
        while ( (cmdlen=recv(http_s1,buf,4095,0))>0 ) 
		{
            buf[cmdlen] = 0;
            if ( strncmp(buf, "GET /",5)!=0 ) break;//serve only get request

            cmd = buf+5;
            char *p = strchr(cmd, ' ');
            if ( p!=NULL ) *p = 0;
            for ( char *p=cmd; *p; p++ ) if ( *p=='+' ) *p=' ';
            fl_decode_uri(cmd);

            if ( *cmd!='?' ) 
			{  //get file
                httpFile(http_s1, cmd);
            }
            else 
			{   //CGI request
                replen = term_command(++cmd, &reply);
                int snplen = 4096 - ( buf - bufp );
                int len = snprintf(buf, snplen, HEADER, replen);
                if ( send(http_s1, buf, len, 0)<0 ) break;
                len = 0;
                while ( replen>0 ) {
                    int pkt_l = replen>8192?8192:replen;
                    len = send(http_s1, reply+len, pkt_l, 0);
                    if ( len<0 ) break;
                    reply+=len;
                    replen-=len;
                }
                if ( len<0 ) break;
            }   
        }
        closesocket(http_s1);
    }
}

static int http_s0 = -1;

void httpd_init()
{
#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,0), &wsadata);
#endif
    http_s0 = socket(AF_INET, SOCK_STREAM, 0);
    if ( http_s0==-1 ) return;

    struct sockaddr_in svraddr;
    int addrsize=sizeof(svraddr);
    memset(&svraddr, 0, addrsize);
    svraddr.sin_family=AF_INET;
    svraddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    short port = 8079;

    while ( ++port<8100 ) 
	{
        svraddr.sin_port=htons(port);
        if ( bind(http_s0, (struct sockaddr*)&svraddr, addrsize)!=-1 )
            break;
    }

    if ( port<8100) 
	{
        if ( listen(http_s0, 1)!=-1){
            std::thread httpThread(httpd, http_s0);
            httpThread.detach();
            httport = port;
            return;
        }
    }
    closesocket(http_s0);
}
void httpd_exit()
{
    closesocket(http_s0);
}
