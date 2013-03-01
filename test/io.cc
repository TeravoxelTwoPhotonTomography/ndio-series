/** \file
    Testing reading of nD volumes from file series.
    @cond TEST

    \todo APPEND test
*/

#include <gtest/gtest.h>
#include "plugins/ndio-series/config.h"
#include "nd.h"

#define countof(e) (sizeof(e)/sizeof(*e))

static
struct _files_t
{ const char  *path;
  nd_type_id_t type;
  size_t       ndim;
  size_t       shape[5];
}
 
file_table[] =
{ // Set a: Should be i16, but is read by mylib as u16
  {NDIO_SERIES_TEST_DATA_PATH"/a/vol.1ch%.tif"  ,nd_u16,3,{620,512,10,1,1}},
#if _MSC_VER
  {NDIO_SERIES_TEST_DATA_PATH"\\b\\vol.0.0000.tif",nd_u8 ,4,{620,512,2,16,1}},
#else
  {NDIO_SERIES_TEST_DATA_PATH"/b/vol.0.0000.tif",nd_u8 ,4,{620,512,2,16,1}},
#endif
  {0}
};

struct Series:public testing::Test
{ void SetUp()
  { ndioAddPluginPath(NDIO_BUILD_ROOT);
  }  
};

TEST_F(Series,OpenClose)
{ struct _files_t *cur;
  // Examples that should fail to open
#if 1
  EXPECT_EQ(NULL,ndioOpen("does_not_exist.im.super.serious","series","r"));
  EXPECT_EQ(NULL,ndioOpen("does_not_exist.im.super.serious","series","w"));
  EXPECT_EQ(NULL,ndioOpen("","series","r"));
  EXPECT_EQ(NULL,ndioOpen("","series","w"));
  EXPECT_EQ(NULL,ndioOpen(NULL,"series","r"));
  EXPECT_EQ(NULL,ndioOpen(NULL,"series","w"));
#endif
  // Examples that should open
  for(cur=file_table;cur->path!=NULL;++cur)
  { ndio_t file=0;
    EXPECT_NE((void*)NULL,file=ndioOpen(cur->path,"series","r"));
    EXPECT_STREQ("series",ndioFormatName(file));
    ndioClose(file);
  }
}

TEST_F(Series,Shape)
{ struct _files_t *cur;
  for(cur=file_table;cur->path!=NULL;++cur)
  { ndio_t file=0;
    nd_t form;
    EXPECT_NE((void*)NULL,file=ndioOpen(cur->path,"series","r"))<<cur->path;
    ASSERT_NE((void*)NULL,form=ndioShape(file))<<ndioError(file)<<"\n\t"<<cur->path;
    EXPECT_EQ(cur->type,ndtype(form))<<cur->path;
    EXPECT_EQ(cur->ndim,ndndim(form))<<cur->path;
    for(size_t i=0;i<cur->ndim;++i)
      EXPECT_EQ(cur->shape[i],ndshape(form)[i])<<cur->path;
    ndfree(form);
    ndioClose(file);
  }
}

TEST_F(Series,Read)
{ struct _files_t *cur;
  for(cur=file_table;cur->path!=NULL;++cur)
  { ndio_t file=0;
    nd_t vol;
    EXPECT_NE((void*)NULL,file=ndioOpen(cur->path,"series","r"));
    ASSERT_NE((void*)NULL, vol=ndioShape(file))<<ndioError(file)<<"\n\t"<<cur->path;
    EXPECT_EQ(vol,ndref(vol,malloc(ndnbytes(vol)),nd_heap));
    EXPECT_EQ(file,ndioRead(file,vol));
    ndfree(vol);
    ndioClose(file);
  }
}

TEST_F(Series,ReadSubarray)
{ struct _files_t *cur;
  for(cur=file_table;cur->path!=NULL;++cur)
  { ndio_t file=0;
    nd_t vol;
    size_t n;
    EXPECT_NE((void*)NULL,file=ndioOpen(cur->path,"series","r"));
    ASSERT_NE((void*)NULL, vol=ndioShape(file))<<ndioError(file)<<"\n\t"<<cur->path;
    // Assume we know the dimensionality of our data and which dimension to iterate over.
    n=ndshape(vol)[2];      // remember the range over which to iterate
    ndShapeSet(vol,2,1); // prep to iterate over 3'rd dimension (e.g. expect WxHxDxC data, read WxHx1XC planes)
    EXPECT_EQ(vol,ndref(vol,malloc(ndnbytes(vol)),nd_heap)); // alloc just enough data      
    { size_t pos[]={0,0,0,0}; // 4d data
      ndio_t a=file;
      for(size_t i=0;i<n && a;++i,++pos[2])
      { ASSERT_EQ(file,a=ndioReadSubarray(file,vol,pos,0))<<ndioError(file); // seek to pos and read, shape limited by vol
      }
    }
    ndfree(vol);
    ndioClose(file);
  }
}

TEST_F(Series,Write)
{ 
  nd_t vol;
  // Read data
  { ndio_t file=0;
    struct _files_t *cur=file_table+1;// Open data set B
    EXPECT_NE((void*)NULL,file=ndioOpen(cur->path,"series","r"));
    ASSERT_NE((void*)NULL, vol=ndioShape(file))<<ndioError(file)<<"\n\t"<<cur->path;
    EXPECT_EQ(vol,ndref(vol,malloc(ndnbytes(vol)),nd_heap));
    ASSERT_EQ(file,ndioRead(file,vol));
    ndioClose(file);
  }

#if 1
  // Transpose colors to last dimension
  { nd_t dst=ndinit();
    ndref(dst,malloc(ndnbytes(vol)),nd_heap);
    ndreshape(ndcast(dst,ndtype(vol)),ndndim(vol),ndshape(vol));
    EXPECT_EQ(dst,ndtranspose(dst,vol,2,3,0,NULL));
    // Cleanup vol
    ndfree(vol);
    vol=dst; // Carry the array out of scope
  }
#endif

  // Write
  { ndio_t file=0;
    EXPECT_NE((void*)NULL,file=ndioOpen("B.%.tif","series","w"));
    EXPECT_NE((void*)NULL,ndioWrite(file,vol));
    ndioClose(file);
  }

  // Cleanup
  if(vol && nddata(vol)) free(nddata(vol));
  ndfree(vol);
}
/// @endcond
