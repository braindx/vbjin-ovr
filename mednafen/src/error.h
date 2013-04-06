#ifndef __MDFN_ERROR_H
#define __MDFN_ERROR_H

#include <errno.h>
#include <string.h>

#ifdef __cplusplus
class MDFN_Error
{
 public:

 MDFN_Error(int errno_code_new, const char *format, ...);
 ~MDFN_Error();

 MDFN_Error(const MDFN_Error &ze_error);

 const char *GetErrorMessage(void);
 int GetErrno(void);

 private:

 int errno_code;
 char *error_message;
};

class ErrnoHolder
{
 public:

 ErrnoHolder()
 {
  //SetErrno(0);
  local_errno = 0;
  local_strerror[0] = 0;
 }

 ErrnoHolder(int the_errno)
 {
  SetErrno(the_errno);
 }

 inline int Errno(void)
 {
  return(local_errno);
 }

 const char *StrError(void)
 {
  return(local_strerror);
 }

 void operator=(int the_errno)
 {
  SetErrno(the_errno);
 }

 private:

 void SetErrno(int the_errno);

 int local_errno;
 char local_strerror[256];
};

#endif

#endif
