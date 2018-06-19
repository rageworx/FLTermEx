//
// "$Id: ssh2.h 3260 2018-06-18 23:48:10 $"
//
// sshHost sftpHost confHost 
//
//	  host implementation for terminal simulator
//    to be used with the Fl_Term widget.
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

#include "Hosts.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <mutex>


#ifndef _SSH2_H_
#define _SSH2_H_

class sshHost : public tcpHost {
private:
	char username[64];
	char password[64];
	char passphrase[64];
	
protected:
	LIBSSH2_SESSION *session;
	LIBSSH2_CHANNEL *channel;
	std::mutex mtx;
	int tunStarted;
	
	int wait_socket();
	int ssh_knownhost();
	int ssh_authentication();

	int scp_read_one(const char *rpath, const char *lpath);
	int scp_write_one(const char *lpath, const char *rpath);
	int tun_local(const char *lpath, const char *rpath);
	int tun_remote(const char *rpath,const char *lpath);

public:
	sshHost(const char *address); 

	void set_user_pass( const char *user, const char *pass ) { 
		if ( *user ) strncpy(username, user, 31); 
		if ( *pass ) strncpy(password, pass, 31); 
	}
	int scp(const char *cmd);
	int scp_read(const char *rpath, const char *lpath);
	int scp_write(const char *lpath, const char *rpath);
	int tunnel(const char *cmd);
	
//	virtual const char *name();
	virtual int type() { return HOST_SSH; }
	virtual int connect();
	virtual int read(char *buf, int len);
	virtual void write(const char *buf, int len);
	virtual void send_size(int sx, int sy);
//	virtual void disconn();				
};

class sftpHost : public sshHost {
private:
	LIBSSH2_SFTP *sftp_session;
	char realpath[1024];
	char homepath[1024];
protected:
	int sftp_lcd(char *path);
	int sftp_cd(char *path);
	int sftp_md(char *path);
	int sftp_rd(char *path);
	int sftp_ls(char *path, int ll=false);
	int sftp_rm(char *path);
	int sftp_ren(char *src, char *dst);
	int sftp_get_one(char *src, char *dst);
	int sftp_get(char *src, char *dst);
	int sftp_put(char *src, char *dst);
	int sftp_put_one(char *src, char *dst);

public:
	sftpHost(const char *address) : sshHost(address) {}
//	virtual const char *name();
	virtual int type() { return HOST_SFTP; }
	virtual int connect();
	virtual int read(char *buf, int len);
	virtual void write(const char *buf, int len){}
	virtual void send_size(int sx, int sy){}
//	virtual void disconn();	
	int sftp(char *p);
};

#define BUFLEN 65536*2
class confHost : public sshHost {
private: 
	LIBSSH2_CHANNEL *channel2;
	char notif[BUFLEN];
	char reply[BUFLEN];
	int rd;
	int rd2;
	int msg_id;
public:
	confHost(const char *address) : sshHost(address)
	{
		if ( port==22 ) port = 830;
	}	

//	virtual const char *name();
	virtual int type() { return HOST_CONF; }
	virtual int connect();
	virtual int connect2();
	virtual int read(char *buf, int len);
	virtual void write(const char *buf, int len);
//	virtual void send_size(int sx, int sy);
//	virtual void disconn();
};

#endif //_SSH2_H_