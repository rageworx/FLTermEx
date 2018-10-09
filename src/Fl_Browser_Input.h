//
// "$Id: Fl_Browser_Input.h 936 2018-08-24 13:48:10 $"
//
// Fl_Input widget extended with auto completion
//
// Copyright 2017-2018 by Yongchao Fan.
//
// This library is free software distributed under GNU LGPL 3.0,
// see the license at:
//
//     https://github.com/zoudaokou/flTerm/blob/master/LICENSE
//
// Please report all bugs and problems on the following page:
//
//     https://github.com/zoudaokou/flTerm/issues/new
//

#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Menu_Window.H>

#ifndef _BROWSER_INPUT_H_
#define _BROWSER_INPUT_H_

class Fl_Browser_Input: public Fl_Input {
private:
	Fl_Menu_Window *browserWin;
	Fl_Browser *browser;
	int id;
public:
	Fl_Browser_Input(int X,int Y,int W,int H,const char* L=0);
	~Fl_Browser_Input()
	{
		browserWin->hide();
	}
	int handle( int e );
	void resize( int X, int Y, int W, int H );
	void add( const char *cmd );
};
#endif  //acInput