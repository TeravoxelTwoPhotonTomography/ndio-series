/**
 * \file
 * An ndio plugin for reading file series.
 *
 * Many common image formats are only good up to two-dimensions plus a limited
 * number of colors which confounds the storage of higher dimensional data; 
 * video formats have similar problems.  However they have the advantage of 
 * being common!  It's easy to inspect those image and video files.
 *
 * This plugin helps support those formats by reading/writing a series of files
 * for the dimensions that exceed the capacity of the individual formats.  For
 * example, to a 5-dimensional array might be written to:
 *
 * \verbatim
   myfile.000.000.mp4
   myfile.000.001.mp4
   myfile.001.000.mp4
   myfile.001.001.mp4
   \endverbatim
 *
 * The <tt>.###.</tt> pattern represents the index on a dimension.  There are 
 * two such fields in the filenames above, and each represents a dimension.
 * Each individual file holds 3-dimensions worth of data, and two extra
 * dimensions are given by the file series.  So these files represent our 5D 
 * array.
 *
 * \todo Allow elements of the path to enumerate a dimension, as opposed to
 *       the filename alone.
 *
 * \todo Seek needs to know field width of filenames. How?
 *       Another option is to maintain a table mapping from positions to filenames
 * \author Nathan Clack
 * \date   Aug 2012
 */
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <re2/re2.h>
#include <cerrno>
#include <iostream>
#include "nd.h"

#define AUTODETECT // turn on filename based detection of file series

#ifdef _MSC_VER
#include "dirent.win.h"
#pragma warning(disable:4996) // security warning
#define snprintf _snprintf
#else
#include <dirent.h>
#endif

/// @cond DEFINES
#ifdef _MSC_VER
#define PATHSEP "\\"
#else
#define PATHSEP "/"
#endif
#define countof(e) (sizeof(e)/sizeof(*e))

#define ENDL                  "\n"
#define LOG(...)              printf(__VA_ARGS__)
#define TRY(e)                do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); breakme();goto Error;}} while(0)
#define TRYMSG(e,msg)         do{if(!(e)) {LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e,msg); goto Error; }}while(0)
#define FAIL(msg)             do{ LOG("%s(%d): %s()"ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,msg); goto Error;} while(0)
#define RESIZE(type,e,nelem)  TRY((e)=(type*)realloc((e),sizeof(type)*(nelem)))
#define NEW(type,e,nelem)     TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define ALLOCA(type,e,nelem)  TRY((e)=(type*)alloca(sizeof(type)*(nelem)))
#define SAFEFREE(e)           if(e){free(e); (e)=NULL;}
#define HERE                  LOG("HERE -- %s(%d): %s()"ENDL,__FILE__,__LINE__,__FUNCTION__)
void breakme() {}
/// @endcond 

//
//  === HELPERS ===
//
typedef std::vector<size_t> TPos;
/**
 * Set read/write mode flags according to mode string.
 */
static
bool parse_mode_string(const char* mode, char *isr, char *isw)
{ const char *c=mode;
  *isr=*isw=0;
  do
  { switch(*c)
    { case 'r': *isr=1; break;
      case 'w': *isw=1; break;
      default:  FAIL("Invalid mode string");
    }
  } while(*++c);
  return true;
Error:
  return false;
}

/** Accumulate minima in \a acc for each vector element. */
static void vmin(TPos &acc, TPos& pos)
{ if(acc.size()!=pos.size())
  { acc=pos;
  } else
  { TPos::iterator iacc,ipos;
    for(iacc=acc.begin(),ipos=pos.begin();iacc!=acc.end();++iacc,++ipos)
    { size_t m=*iacc,p=*ipos;
      *iacc=(m<p)?m:p;
    }
  }
}

/** Accumulate maxima in \a acc for each vector element. */
static void vmax(TPos &acc, TPos& pos)
{ if(acc.size()!=pos.size())
  { acc=pos;
  } else
  { TPos::iterator iacc,ipos;
    for(iacc=acc.begin(),ipos=pos.begin();iacc!=acc.end();++iacc,++ipos)
    { size_t m=*iacc,p=*ipos;
      *iacc=(m>p)?m:p;
    }
  }
}

/** Add \a pos to \a acc elementwise. <tt>acc+=pos</tt> */
static void vadd(std::vector<size_t> &acc, TPos& pos)
{ std::vector<size_t>::iterator iacc;
  TPos::iterator ipos;
  for(iacc=acc.begin(),ipos=pos.begin();iacc!=acc.end();++iacc,++ipos)
    *iacc+=*ipos;
}

/** Assemble full path to an ndio_t file and open it. */
static ndio_t openfile(const std::string& path, const char* fname)
{ std::string name(path);
  name.append(PATHSEP);
  name.append(fname);
  return ndioOpen(name.c_str(),NULL,"r");
}

/** Determine the shape of the array stored in the file specied by \a path
    and \a fname.
*/
static nd_t get_file_shape(const std::string& path, const char* fname)
{ nd_t shape=0; 
  ndio_t file=0;
  TRY(file=openfile(path,fname));
  shape=ndioShape(file);
  ndioClose(file);
Error:
  return shape;
}

//
// === CONTEXT CLASS ===
//

/**
 * File context for ndio-series.
 */
struct series_t 
{
  std::string path_,     ///< the folder to search/put files
              pattern_;  ///< the filename pattern, should not include path elements
  unsigned ndim_;        ///< the number of dimensions represented in the pattern
  char     isr_,isw_;    ///< mode flags (readable, writeable)
  size_t   last_;        ///< keeps track of last written position for appending
  int64_t  fdim_;        ///< number of dimensions for each file.  Not known until canseek() call.

  typedef std::string           TName;
  typedef std::map<TPos,TName>  TSeekTable;

  TSeekTable seektable_;

  RE2 ptn_field;  ///< Recognizes the "%" style filename patterns
  RE2 eg_field;   ///< Recognizes the "*.000.000.ext" example filename patterns.

  /**
   * Opens a file series from the filename pattern in \a path
   * according to the mode \a mode.
   * \param[in] path  A std::string with a valid filename pattern.
   * \param[in] mode  May be "r", "w", or "rw".
   */
  series_t(const std::string& path, const char* mode)
  : ndim_(0)
  , isr_(0)
  , isw_(0)
  , last_(0)
  , fdim_(-1)
  , ptn_field("%+")
  , eg_field("\\.(\\d+)")
  { char t[1024];
    std::string p(path);
    size_t n;
    TRY(parse_mode_string(mode,&isr_,&isw_));
#ifdef _MSC_VER
    GetFullPathName(path.c_str(),1024,t,NULL); // normalizes slashes for windows
    p.assign(t);
#endif
    n=p.rfind(PATHSEP[0]);
    { n=(n>=p.size())?0:n; // if not found set to 0
      path_=p.substr(0,n); // if PATHSEP not found will be ""
      std::string name((n==0)?p:p.substr(n+1));
      if(!gen_pattern_(name,ptn_field,"(\\\\d+)"))
        gen_pattern_(name,eg_field,".(\\\\d+)");
#if 0
      std::cout << "  INPUT: "<<path<<std::endl
                << "   PATH: "<<path_<<std::endl
                << "PATTERN: "<<pattern_<<std::endl
                << "   NDIM: "<<ndim_<<std::endl; 
#endif                
    }
  Error:
    ;
  }

  /** Check validity. \returns true if series_t was opened properly, otherwise 0. */
  bool isok() { return ndim_>0; }

  /**
   * Parse \a name according to \a pattern_ to extract the position of 
   * the file according to the dimensions encoded in the filename.
   *
   * \param[in]   name  The filename to parse. Should not include the path to 
   *                    the file.
   * \param[out]  pos   The position according to the dimension fields encoded
   *                    in \a name.  Only valid if the function returns true.
   * \returns true on success, otherwise false.   
   */
  bool parse(const std::string& name, TPos& pos)
  { TRY(isok()); 
    { unsigned p[10];
      RE2::Arg *args[10];
      TRY(ndim_<countof(args));
      for(unsigned i=0;i<ndim_;++i)
        args[i]=new RE2::Arg(p+i);
#if 0
      LOG("%s(%d): %s()"ENDL "%s\t%s\t%u"ENDL,
        __FILE__,__LINE__,__FUNCTION__,
        name.c_str(),pattern_.c_str(),ndim_);
#endif
      if(RE2::FullMatchN(name,pattern_,args,ndim_))
      { pos.clear();
        pos.reserve(ndim_);
        for(unsigned i=0;i<ndim_;++i)
          pos.push_back(p[i]);
        return true;
      }
      for(unsigned i=0;i<ndim_;++i)
        delete args[i];
    }
Error:
    return false;
  }

  /**
   * Generates a filename for writing corresponding to the position at \a ipos.
   * IMPORTANT: For writing ONLY.
   * 
   * \param[out]  out   A std::string reference used for the output name.
   * \param[in]   ipos  A std::vector with the position of the filename.
   */
  bool makename(std::string& out,std::vector<size_t> &ipos)
  { char buf[128]={0};
    std::string t=pattern_;
    ipos.back()+=last_;
    //(*ipos.rend())+=last_;
    for (std::vector<size_t>::iterator it = ipos.begin(); it != ipos.end(); ++it)
    { snprintf(buf,countof(buf),"%llu",(unsigned long long)*it);
      TRY(RE2::Replace(&t,"\\(\\\\d\\+\\)",buf));
    }
    ipos.back()-=last_;
    out.clear();
    if(!path_.empty())
    { out+=path_;
      out+=PATHSEP;
    }
    out+=t;
    return 1;
Error:
    return 0;
  }

  /**
   * Probes the series' path for matching files and determines the maximum
   * and minimum positions indicated by the filenames.
   *
   * A minimum gets read to 0 in the corresponding dimension.
   *
   * The shape of the array along a dimension is the distance between the
   * maximum and minimum.
   *
   * \param[out] mn   A std::vector with the minima.
   * \param[out] mx   A std::vector with the maxima.
   */
  bool minmax(TPos& mn, TPos& mx)
  { DIR *dir=0;
    struct dirent *ent=0;
    TRYMSG(dir=opendir(path_.c_str()),strerror(errno));
    while((ent=readdir(dir))!=NULL)
    { TPos pos;
      if(parse(ent->d_name,pos))
      { vmin(mn,pos);
        vmax(mx,pos);
      }
    }
    closedir(dir);
    return true;
  Error:
    LOG("\t%s"ENDL,path_.c_str());
    return false;
  }

  /** \returns the shape of the first matching file in a series as an nd_t. */
  nd_t single_file_shape()
  { DIR *dir=0;
    struct dirent *ent;
    TRYMSG(dir=opendir(path_.c_str()),strerror(errno));
    while((ent=readdir(dir))!=NULL)
    { TPos pos;      
      if(parse(ent->d_name,pos))
      { nd_t shape=get_file_shape(path_,ent->d_name);
        closedir(dir);
        return shape;
      }
    }
  Error:
    return 0;  
  }

  /**
   * Sets \a out to the filename expected for position \a ipos.
   * \returns true on success, otherwise false.
   */
  bool find(std::string& out,TPos ipos)
  { TSeekTable::iterator it;
    if(seektable_.empty()) 
      TRY(build_seek_table_());
    TRY((it=seektable_.find(ipos))!=seektable_.end());
    out.clear();
    if(!path_.empty())
    { out+=path_;
      out+=PATHSEP;
    }
    out+=it->second;
#if 0
    std::cout << out << std::endl;
#endif
    return true;
Error:
    return false;
  }
  /** Searches for the first file, opens it, and queries it's
      seekable dimensions if applicable.  Otherwise, returns 1.
      Dimensions corresponding to whole file's are seekable.
  */
  unsigned canseek(size_t idim)
  { DIR *dir=0;
    struct dirent *ent;
    ndio_t file=0;
    nd_t shape=0; // I wish I didn't have to get this each time
    TRYMSG(dir=opendir(path_.c_str()),strerror(errno));
    while((ent=readdir(dir))!=NULL)
    { TPos pos;      
      if(parse(ent->d_name,pos))
      { unsigned out;
        TRY(file=openfile(path_,ent->d_name));
        TRY(shape=ndioShape(file));
        fdim_=ndndim(shape);
        if(idim<ndndim(shape))
          out=ndioCanSeek(file,idim);
        else
          out=1;
        ndfree(shape);
        ndioClose(file);
        closedir(dir);
        return out;
      }
    }
  Error:
    ndioClose(file);
    ndfree(shape);
    return 0;
  }

  private:
    /**
     * Changes \a name if a pattern is found, but otherwise leaves it 
     * untouched.
     * \returns true if a pattern is detected, otherwise false.
     */ 
    bool gen_pattern_(std::string& name, const RE2& re, const char* repl)
    { while(RE2::Replace(&name,re,repl))
        ++ndim_;
      if(ndim_) pattern_=name;
      return ndim_>0;
    }

    /**
     * Build seek table by searching through the \a path_ and locating parsable
     * files.  The parsed positions are inserted into the \a seektable_.    
     * \returns true on success, otherwise false.
     */
    bool build_seek_table_()
    { DIR *dir=0;
      struct dirent *ent;
      TRYMSG(dir=opendir(path_.c_str()),strerror(errno));
      seektable_.clear();
      while((ent=readdir(dir))!=NULL)
      { TPos pos;
        if(parse(ent->d_name,pos))
          seektable_[pos]=ent->d_name;
      }
      return true;
Error:
      return false;
    }
};

/** The format name.
    Use the format name to select this format.
*/
static const char* series_fmt_name(void) { return "series"; }

/** This format is disabled for autodetection.
    \returns 0.
 */
static unsigned series_is_fmt(const char* path, const char *mode)
{
#ifdef AUTODETECT
  // Detect filenames with % placeholders.
  // This doesn't cover all valid series names, but it does cover the ones
  // expected to be unique to this format.
  char t[1024];
  std::string p(path);  
#ifdef _MSC_VER
  GetFullPathName(path,1024,t,NULL); // normalizes slashes for windows
  p.assign(t);
#endif
  size_t n=p.rfind(PATHSEP[0]);
  n=(n>=p.size())?0:n; // if not found set to 0
  std::string name((n==0)?p:p.substr(n+1));
  return name.find('%')<p.size();
#else
  return 0;
#endif
}


/**
 * Opens a file series.
 *
 * The file name has to have fields corresponding to each dimension.
 * There are two file name patterns that may be used:
 *
 * 1. An example file from the series.
 *    The filename must conform to a prescribed pattern.
 * 
 *    For example: <tt>myfile.123.45.tif</tt>.
 *    This particular file would get loaded to position <tt>(...,123,45)</tt> 
 *    (where the elipses indicate the dimensions in the tif).  The series would 
 *    look for other tif files in the same directory with the same number of 
 *    fields.
 *
 * 2. A "pattern" filename where "%" symbols are used as placeholders for the
 *    dimension fields.
 *
 *     Example: <tt>myfile.%.%.tif</tt> would find/write files like the one 
 *              in the above example.
 *
 *     Example: <tt>1231%2353%351345.mp4</tt> would find/write files like
 *              <tt>12310002353111351345.mp4</tt> with a position of
 *              <tt>(...,0,111)</tt>.
 *
 * The number of dimensions to write to a series is infered from the filename.
 * All the examples above have use two dimensions in the series.  The container
 * used for individual members of the series must be able to hold the other 
 * dimensions.  If it can't, the write will fail.
 * 
 * \param[in]   path    File name as a null terminated string.
 * \param[in]   mode    Mode string: may be "r" or "w".
 * \returns 0 on error, otherwise a file context pointer.
 */
static void* series_open(const char* path, const char* mode)
{ series_t *out=0;
  TRY(out=new series_t(path,mode));
  TRY(out->isok());
  return out;
Error:
  LOG("%s(%d): %s()"ENDL "\tCould not open"ENDL "\t\t%s"ENDL "\t\twith mode \"%s\""ENDL,
      __FILE__,__LINE__,__FUNCTION__,path?path:"(null)",mode?mode:"(null)");
  if(out) delete out;
  return 0;
}

/** Releases resources */
static void series_close(ndio_t file)
{ series_t *self=(series_t*)ndioContext(file);
  delete self;
}

/**
 * Iterate over file's in the path recording min and max's for dims in names.
 * Open one to get the shape.
 */
static nd_t series_shape(ndio_t file)
{ series_t *self=(series_t*)ndioContext(file);
  nd_t shape=0;
  TPos mn,mx;
  TRY(self->minmax(mn,mx));
  TRY(shape=self->single_file_shape());
  { size_t i,o=ndndim(shape);
    ndInsertDim(shape,(unsigned)(o+mx.size()-1));
    for(i=0;i<mn.size();++i)
      ndShapeSet(shape,(unsigned)(o+i),mx[i]-mn[i]+1);
  }
  return shape;
Error:
  return 0;
}

/**
 * Reads a file series into \a dst.
 */
static unsigned series_read(ndio_t file,nd_t dst)
{ series_t *self=(series_t*)ndioContext(file);
  const size_t o=ndndim(dst)-self->ndim_;
  DIR *dir;
  struct dirent *ent;
  TPos mn,mx;
  TRY(self->minmax(mn,mx));
  TRY(self->isr_);  
  TRYMSG(dir=opendir(self->path_.c_str()),strerror(errno));
  while((ent=readdir(dir))!=NULL)
  { TPos v;
    ndio_t file=0;
    if(!self->parse(ent->d_name,v))               continue;
    if(!(file=openfile(self->path_,ent->d_name))) continue;
    for(size_t i=0;i<self->ndim_;++i) //  set the read position
      ndoffset(dst,(unsigned)(o+i),v[i]-mn[i]);
    ndioClose(ndioRead(file,dst));
    for(size_t i=0;i<self->ndim_;++i) //reset the read position
      ndoffset(dst,(unsigned)(o+i),-(int64_t)v[i]+(int64_t)mn[i]);
  }
  return 1;
Error:
  return 0;
}

// helpers for the write function
/// (for writing) set offset for writing a sub-array 
static void setpos(nd_t src,const size_t o,const std::vector<size_t>& ipos)
{ for(size_t i=0;i<ipos.size();++i)
    ndoffset(src,(unsigned)(o+i),ipos[i]);
}
/// (for writing) Undo setpos() by negating the offset for writing a sub-array 
static void unsetpos(nd_t src,const size_t o,const std::vector<size_t>& ipos)
{ for(size_t i=0;i<ipos.size();++i)
    ndoffset(src,(unsigned)(o+i),-(int64_t)ipos[i]);
}
/// (for writing) Maybe increment sub-array position, otherwise stop iteration.
static bool inc(nd_t src,size_t o,std::vector<size_t> &ipos)
{ int kdim=(int)ipos.size()-1;
  while(kdim>=0 && ipos[kdim]==ndshape(src)[o+kdim]-1) // carry
    ipos[kdim--]=0;
  if(kdim<0) return 0;
  ipos[kdim]++;
#if 0
  for(size_t i=0;i<ipos.size();++i)
    printf("%5zu",ipos[i]);
  printf(ENDL);
#endif
  return 1;
}

/**
 * Write a file series.
 */
static unsigned series_write(ndio_t file, nd_t src)
{ series_t *self=(series_t*)ndioContext(file);
  size_t o;
  std::string outname;
  std::vector<size_t> ipos;
  TRY(self->isw_); // is writable?
  ipos.assign(self->ndim_,0);
  o=ndndim(src)-1;
  do
  { setpos(src,o,ipos);
    ndreshape(src,(unsigned)(o-self->ndim_+1),ndshape(src)); // drop dimensionality
    TRY(self->makename(outname,ipos));
    ndioClose(ndioWrite(ndioOpen(outname.c_str(),NULL,"w"),src));
    ndreshape(src,(unsigned)(o+1),ndshape(src));             // restory dimensionality
    unsetpos(src,o,ipos);
  } while (inc(src,o,ipos));
  self->last_+=ipos.back();
  return 1;
Error:
  return 0;
}

/**
 * Seek
 */
static unsigned series_seek(ndio_t file, nd_t dst, size_t *pos)
{ series_t *self=(series_t*)ndioContext(file);
  std::vector<size_t> ipos;
  std::string outname;
  TPos mn,mx;
  ndio_t t=0;
  size_t *shape;
  size_t odim=ndndim(dst);
  ALLOCA(size_t,shape,ndndim(dst));
  memcpy(shape,ndshape(dst),ndndim(dst)*sizeof(size_t)); // save dst shape
  for(size_t i=0;i<ndndim(dst);++i)
    if(self->canseek(i))
      ndshape(dst)[i]=1;       // reduce dst shape to 1 on seekable dims...don't change strides
  TRY(self->minmax(mn,mx));    // OPTIMIZE: recomputing minmax each call is wasteful
  TRY(self->fdim_>0);
  ipos.insert(ipos.begin(),
              pos+self->fdim_,
              pos+self->fdim_+self->ndim_);
  vadd(ipos,mn);
  TRY(self->find(outname,ipos));
  { TRY(t=ndioOpen(outname.c_str(),NULL,"r"));
    TRY(ndreshape(dst,(unsigned)(self->fdim_),ndshape(dst))); // temporarily lower dimension
    TRY(ndioReadSubarray(t,dst,pos,NULL));
    ndioClose(t);t=0;
    TRY(ndreshape(dst,(unsigned)odim,ndshape(dst))); // restore dimensionality
  }
  memcpy(ndshape(dst),shape,ndndim(dst)*sizeof(size_t)); // restore dst shape
  return 1;
Error:
  memcpy(ndshape(dst),shape,ndndim(dst)*sizeof(size_t)); // restore dst shape
  if(ndioError(t))
  { LOG("\t[Sub file error]"ENDL "\t\tFile: %s"ENDL "\t\t%s"ENDL,
        outname.c_str(),ndioError(t));
    ndioClose(t);
  }
  return 0;
}

/**
 * Query which dimensions ares seekable.
 */
static unsigned series_canseek(ndio_t file, size_t idim)
{ series_t *self=(series_t*)ndioContext(file);
  return self->canseek(idim);
}

//
// === EXPORT ===
//

/// @cond DEFINES
#ifdef _MSC_VER
#define shared extern "C" __declspec(dllexport)
#else
#define shared extern "C"
#endif
/// @endcond

#include "src/io/interface.h"
/// Interface function for the ndio-series plugin.
shared const ndio_fmt_t* ndio_get_format_api(void)
{ 
  static ndio_fmt_t api=
  { series_fmt_name,
    series_is_fmt,
    series_open,
    series_close,
    series_shape,
    series_read,
    series_write,
    NULL, //set
    NULL, //get
    series_canseek,
    series_seek,
    NULL, //subarray
    ndioAddPlugin,
    NULL 
  };
  // make sure init happened ok
  //TRY(series_t::ptn_field.ok());
  //TRY(series_t::eg_field.ok());
  return &api;
Error:
  return 0;
}
