#ifndef _NJCLIENT_H_
#define _NJCLIENT_H_

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <memory.h>
#endif
#include <stdio.h>
#include <time.h>
#include "../WDL/string.h"
#include "../WDL/ptrlist.h"
#include "../WDL/jnetlib/jnetlib.h"
#include "../WDL/sha.h"
#include "../WDL/rng.h"
#include "../WDL/mutex.h"

#include "../WDL/wavwrite.h"

#include "netmsg.h"


#define MAKE_NJ_FOURCC(A,B,C,D) ((A) | ((B)<<8) | ((C)<<16) | ((D)<<24))



#define NJ_ENCODER VorbisEncoder
#define NJ_ENCODER_FMT_TYPE MAKE_NJ_FOURCC('O','G','G','v')
#define NJ_DECODER VorbisDecoder
#include "../WDL/vorbisencdec.h"


class RemoteDownload;
class RemoteUser;
class RemoteUser_Channel;
class Local_Channel;
class DecodeState;

class NJClient
{
  friend RemoteDownload;
public:
  NJClient();
  ~NJClient();

  void Connect(char *host, char *user, char *pass);
  void Disconnect();

  // call Run() from your main (UI) thread
  int Run();// returns nonzero if sleep is OK

  char *GetErrorStr() { return m_errstr.Get(); }

  int IsAudioRunning() { return m_audio_enable; }
  // call AudioProc, (and only AudioProc) from your audio thread
  void AudioProc(float **inbuf, int innch, float **outbuf, int outnch, int len, int srate); // len is number of sample pairs or samples


  // basic configuration (these will actually go away soon)
  int   config_autosubscribe;
  int   config_savelocalaudio; // set 1 to save compressed files, set to 2 to save .wav files as well
  float config_metronome,config_metronome_pan; // volume of metronome
  bool  config_metronome_mute;
  float config_mastervolume,config_masterpan; // master volume
  bool  config_mastermute;
  int   config_debug_level; 


  float output_peaklevel;

  enum { NJC_STATUS_DISCONNECTED=-3,NJC_STATUS_INVALIDAUTH=-2, NJC_STATUS_CANTCONNECT=-1, NJC_STATUS_OK=0, NJC_STATUS_PRECONNECT};
  int GetStatus();

  void SetWorkDir(char *path);
  char *GetWorkDir() { return m_workdir.Get(); }

  char *GetUserName() { return m_user.Get(); }
  char *GetHostName() { return m_host.Get(); }

  float GetActualBPM() { return (float) (60.0*m_active_bpi*m_srate/(double)m_interval_length); }
  int GetBPI() { return m_active_bpi; }
  void GetPosition(int *pos, int *length);  // positions in samples
  int GetLoopCount() { return m_loopcnt; }

  int HasUserInfoChanged() { if (m_userinfochange) { m_userinfochange=0; return 1; } return 0; }
  int GetNumUsers() { return m_remoteusers.GetSize(); }
  char *GetUserState(int idx, float *vol=0, float *pan=0, bool *mute=0);
  void SetUserState(int idx, bool setvol, float vol, bool setpan, float pan, bool setmute, bool mute);

  float GetUserChannelPeak(int useridx, int channelidx);
  char *GetUserChannelState(int useridx, int channelidx, bool *sub=0, float *vol=0, float *pan=0, bool *mute=0, bool *solo=0);
  void SetUserChannelState(int useridx, int channelidx, bool setsub, bool sub, bool setvol, float vol, bool setpan, float pan, bool setmute, bool mute, bool setsolo, bool solo);
  int EnumUserChannels(int useridx, int i); // returns <0 if out of channels. start with i=0, and go upwards

  void DeleteLocalChannel(int ch);
  int EnumLocalChannels(int i);
  float GetLocalChannelPeak(int ch);
  void SetLocalChannelProcessor(int ch, void (*cbf)(float *, int ns, void *), void *inst);
  void GetLocalChannelProcessor(int ch, void **func, void **inst);
  void SetLocalChannelInfo(int ch, char *name, bool setsrcch, int srcch, bool setbitrate, int bitrate, bool setbcast, bool broadcast);
  char *GetLocalChannelInfo(int ch, int *srcch, int *bitrate, bool *broadcast);
  void SetLocalChannelMonitoring(int ch, bool setvol, float vol, bool setpan, float pan, bool setmute, bool mute, bool setsolo, bool solo);
  int GetLocalChannelMonitoring(int ch, float *vol, float *pan, bool *mute, bool *solo); // 0 on success
  void NotifyServerOfChannelChange(); // call after any SetLocalChannel* that occur after initial connect

  int IsASoloActive() { return m_issoloactive; }

  void SetLogFile(char *name=NULL);

  WaveWriter *waveWrite;

  void makeFilenameFromGuid(WDL_String *s, unsigned char *guid);

  int LicenseAgreement_User32;
  int (*LicenseAgreementCallback)(int user32, char *licensetext); // return TRUE if user accepts


  // messages you can send:
  // "MSG" "text"  - broadcast "text" to everybody
  // "PRIVMSG" "username" "text"  - send text to "username"
  void ChatMessage_Send(char *parm1, char *parm2, char *parm3=NULL, char *parm4=NULL, char *parm5=NULL);

  // messages you can receive from this:
  // "MSG" "user" "text"   - message from user to everybody (including you!), or if user is empty, from the server
  // "PRIVMSG "user" "text"   - private message from user

  // usernames are not case sensitive, but message names ARE.

  // note that nparms is the MAX number of parms, you still can get NULL parms entries in there (though rarely)
  void (*ChatMessage_Callback)(int user32, NJClient *inst, char **parms, int nparms); 
  int ChatMessage_User32;

protected:

  void updateBPMinfo(int bpm, int bpi);
  void process_samples(float **inbuf, int innch, float **outbuf, int outnch, int len, int srate, int offset);
  void on_new_interval();

  void writeLog(char *fmt, ...);

  WDL_String m_errstr;

  WDL_String m_workdir;
  int m_status;
  FILE *m_logFile;

  WDL_String m_user, m_pass, m_host;

  int m_in_auth;
  int m_bpm,m_bpi;
  int m_beatinfo_updated;
  int m_audio_enable;
  int m_srate;
  int m_userinfochange;
  int m_issoloactive;

  int m_loopcnt;
  int m_active_bpm, m_active_bpi;
  int m_interval_length;
  int m_interval_pos, m_metronome_state, m_metronome_tmp,m_metronome_interval;
  double m_metronome_pos;


  WDL_PtrList<Local_Channel> m_locchannels;

  void mixInChannel(bool muted, float vol, float pan, DecodeState *chan, float **outbuf, int len, int srate, int outnch, int offs);

  WDL_Mutex m_users_cs, m_locchan_cs, m_log_cs, m_misc_cs;
  Net_Connection *m_netcon;
  WDL_PtrList<RemoteUser> m_remoteusers;
  WDL_PtrList<RemoteDownload> m_downloads;

  WDL_HeapBuf tmpblock;
};


class DecodeState
{
  public:
    DecodeState() : decode_fp(0), decode_codec(0), dump_samples(0),
                                           decode_samplesout(0), resample_state(0.0), decode_peak_vol(0.0)
    { 
      memset(decode_last_guid,0,sizeof(decode_last_guid));
    }
    ~DecodeState()
    {
      delete decode_codec;
      decode_codec=0;
      if (decode_fp) fclose(decode_fp);
      decode_fp=0;
    }

    double decode_peak_vol;

    FILE *decode_fp;
    NJ_DECODER *decode_codec;
    int decode_samplesout;
    int dump_samples;
    unsigned char decode_last_guid[16];
    double resample_state;

};


#define MAX_USER_CHANNELS 32
class RemoteUser_Channel
{
  public:
    RemoteUser_Channel();
    ~RemoteUser_Channel();

    float volume, pan;

    unsigned char cur_guid[16];
    WDL_String name;


    // decode/mixer state, used by mixer
    DecodeState ds;

};

class RemoteUser
{
public:
  RemoteUser() : muted(0), volume(1.0f), pan(0.0f), submask(0), mutedmask(0), solomask(0), chanpresentmask(0) { }
  ~RemoteUser() { }

  bool muted;
  float volume;
  float pan;
  WDL_String name;
  int submask;
  int chanpresentmask;
  int mutedmask;
  int solomask;
  RemoteUser_Channel channels[MAX_USER_CHANNELS];
};


#define DOWNLOAD_TIMEOUT 8
class RemoteDownload
{
public:
  RemoteDownload();
  ~RemoteDownload();

  void Close();
  void Open(NJClient *parent, unsigned int fourcc);
  void Write(void *buf, int len);
  void startPlaying(int force=0); // call this with 1 to make sure it gets played ASAP, or let RemoteDownload call it automatically

  time_t last_time;
  unsigned char guid[16];

  // set chidx to -1 to disable the user guid updating
  int chidx;
  WDL_String username;

private:
  NJClient *m_parent;
  FILE *fp;
};



#define MAX_LOCAL_CHANNELS 32

class Local_Channel
{
public:
  Local_Channel();
  ~Local_Channel();

  void AddBlock(float *samples, int len);
  int GetBlock(WDL_HeapBuf **b); // return 0 if got one, 1 if none avail
  void DisposeBlock(WDL_HeapBuf *b);

  int channel_idx;

  int src_channel; // 0 or 1
  int bitrate;

  float volume;
  float pan;
  bool muted;
  bool solo;

  //?
  // mode flag. 0=silence, 1=broadcasting, 2=loop last
  bool broadcasting; //takes effect next loop



  // internal state
  bool bcast_active;


  void (*cbf)(float *, int ns, void *);
  void *cbf_inst;

  double decode_peak_vol;
  WDL_Mutex m_cs;
  bool m_need_header;
  WDL_Queue m_samplequeue; // a list of pointers, with NULL to define spaces
  WDL_PtrList<WDL_HeapBuf> m_emptybufs;
  NJ_ENCODER *m_enc;
  int m_enc_bitrate_used;
  Net_Message *m_enc_header_needsend;
  
  WDL_String name;
  RemoteDownload m_curwritefile;
  WaveWriter *m_wavewritefile;

  //DecodeState too, eventually
};


#endif//_NJCLIENT_H_
