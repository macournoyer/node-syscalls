#include <node.h>
#include "nan.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace v8;

#define SYS_ERROR() Nan::ThrowError(strerror(errno))

NAN_METHOD(Socket) {
  if (info.Length() != 3) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting domain, type, protocol.");
  }

  if (!info[0]->IsNumber() || !info[1]->IsNumber() || !info[2]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of arguments. Expecting numbers");
  }
  
  int domain = info[0]->NumberValue();
  int type = info[1]->NumberValue();
  int protocol = info[2]->NumberValue();
  
  int fd = socket(domain, type, protocol);
  if (fd < 0) return SYS_ERROR();
  
  int sock_flags = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sock_flags, sizeof(sock_flags));
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().Set(Nan::New<Number>(fd));
}

NAN_METHOD(Fcntl) {
  if (info.Length() != 3) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD, command, value.");
  }

  if (!info[0]->IsNumber() || !info[1]->IsNumber() || !info[2]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of arguments. Expecting numbers");
  }
  
  int fd = info[0]->NumberValue();
  int cmd = info[1]->NumberValue();
  int val = info[2]->NumberValue();
  
  int ret = fcntl(fd, cmd, val);
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().Set(Nan::New<Number>(ret));
}

NAN_METHOD(Connect) {
  if (info.Length() != 3) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD, port, address.");
  }

  if (!info[0]->IsNumber() || !info[1]->IsNumber() || !info[2]->IsString()) {
    return Nan::ThrowTypeError("Wrong type of arguments. Expecting number, number, string");
  }
  
  int fd = info[0]->NumberValue();
  int port = info[1]->NumberValue();
  Nan::AsciiString addr_str(info[2]);
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(*addr_str);
  int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0 && errno != EINPROGRESS) return SYS_ERROR();
  
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Bind) {
  if (info.Length() != 3) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD, port, address.");
  }

  if (!info[0]->IsNumber() || !info[1]->IsNumber() || !info[2]->IsString()) {
    return Nan::ThrowTypeError("Wrong type of arguments. Expecting number, number, string");
  }
  
  int fd = info[0]->NumberValue();
  int port = info[1]->NumberValue();
  NanAsciiString addr_str(info[2]);
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(*addr_str);
  int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Listen) {
  if (info.Length() != 2) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD, backlog");
  }

  if (!info[0]->IsNumber() || !info[1]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of argument. Expecting number, number");
  }
  
  int fd = info[0]->NumberValue();
  int backlog = info[1]->NumberValue();
  
  int ret = listen(fd, backlog);
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Accept) {
  if (info.Length() != 1) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD.");
  }

  if (!info[0]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of argument. Expecting number");
  }
  
  int fd = info[0]->NumberValue();
  
  struct sockaddr addr;
  socklen_t size = sizeof(addr);
  int cfd;
  while ((cfd = accept(fd, &addr, &size)) == -1 && errno == EINTR) continue;
  if (cfd < 0) return SYS_ERROR();
  
  info.GetReturnValue().Set(Nan::New<Number>(cfd));
}

NAN_METHOD(Select) {
  if (info.Length() < 3 || info.Length() > 4) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting readables, writables, exceptionals[, timeout].");
  }

  if (!info[0]->IsArray() || !info[1]->IsArray() || !info[2]->IsArray() ||
      (info.Length() == 4 && !info[3]->IsNumber())) {
    return Nan::ThrowTypeError("Wrong type of arguments. Expecting array, array, array[, number]");
  }
  
  int nfds = 0; // Max fd+1 to watch
  fd_set fds[3];
  
  // Convert info to fd_sets. fds[0] = read fds, fds[1] = write fds, fds[2] = error fds.
  unsigned set_i, i;
  for(set_i = 0; set_i < 3; ++set_i) {
    FD_ZERO(&fds[set_i]);
    Handle<Array> array = Handle<Array>::Cast(info[set_i]);
    for(i = 0; i < array->Length(); ++i) {
      int fd = array->Get(i)->NumberValue();
      if (fd + 1 > nfds) nfds = fd + 1;
      FD_SET(fd, &fds[set_i]);
    }
  }
  
  // Set the timeout (in sec)
  struct timeval timeout, *timeoutp = 0;
  if (info.Length() == 4) {
    timeout.tv_usec = 0;
    timeout.tv_sec = info[3]->NumberValue();
    timeoutp = &timeout;
  }
  
  // Make the call!
  // EINTR error means it got interupted by a process signal. We simply retry.
  int ret;
  while ((ret = select(nfds, &fds[0], &fds[1], &fds[2], timeoutp)) == -1 &&
         errno == EINTR) continue;
  if (ret < 0) return SYS_ERROR();
  
  // Convert the modified sets to arrays.
  Local<Array> retfds[3];
  for(set_i = 0; set_i < 3; ++set_i) {
    unsigned ret_i = 0;
    retfds[set_i] = Nan::New<Array>();
    Handle<Array> array = Handle<Array>::Cast(info[set_i]);
    for(i = 0; i < array->Length(); ++i) {
      int fd = array->Get(i)->NumberValue();
      if (FD_ISSET(fd, &fds[set_i])) {
        retfds[set_i]->Set(ret_i++, Nan::New<Number>(fd));
      }
    }
  }
  
  Local<Array> retarray = Nan::New<Array>(3);
  retarray->Set(0, retfds[0]);
  retarray->Set(1, retfds[1]);
  retarray->Set(2, retfds[2]);

  info.GetReturnValue().Set(retarray);
}

NAN_METHOD(Close) {
  if (info.Length() != 1) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD.");
  }

  if (!info[0]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of argument. Expecting number");
  }
  
  int fd = info[0]->NumberValue();
  
  int ret = close(fd);
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Read) {
  if (info.Length() != 2) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD, number of bytes.");
  }

  if (!info[0]->IsNumber() || !info[0]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of argument. Expecting number, number.");
  }
  
  int fd = info[0]->NumberValue();
  int nbyte = info[1]->NumberValue();
  char *buf = (char *)malloc(nbyte);
  
  int ret = read(fd, buf, nbyte);
  if (ret < 0) {
    free(buf);
    return SYS_ERROR();
  }
  
  Local<String> str = Nan::New<String>(buf, ret);
  free(buf);
  
  info.GetReturnValue().Set(str);
}

NAN_METHOD(Write) {
  if (info.Length() < 2 || info.Length() > 3) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting FD, data.");
  }

  if (!info[0]->IsNumber() || !info[1]->IsString()) {
    return Nan::ThrowTypeError("Wrong type of argument. Expecting number, string.");
  }
  
  int fd = info[0]->NumberValue();
  Local<String> str = info[1]->ToString();
  NanAsciiString buf(info[1]);
  int nbyte = str->Length();
  
  int ret = write(fd, *buf, nbyte);
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Fork) {
  int ret = fork();
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().Set(Nan::New<Number>(ret));
}

NAN_METHOD(Getpid) {
  info.GetReturnValue().Set(Nan::New<Number>(getpid()));
}

NAN_METHOD(Waitpid) {
  pid_t pid = -1; // Default to waiting for all child processes.
  if (info.Length() >= 1) {
    pid = info[0]->NumberValue();
  }
  
  int options = 0;
  if (info.Length() >= 2) {
    options = info[1]->NumberValue();
  }
    
  int status;
  
  int ret = waitpid(pid, &status, options);
  if (ret < 0) return SYS_ERROR();
  
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Open) {
  if (info.Length() != 2) {
    return Nan::ThrowTypeError("Wrong number of arguments. Expecting path, flags.");
  }

  if (!info[0]->IsString() || !info[1]->IsNumber()) {
    return Nan::ThrowTypeError("Wrong type of argument. Expecting string, number.");
  }
  
  NanAsciiString path(info[0]);
  int flags = info[1]->NumberValue();
  
  int fd = open(*path, flags);
  if (fd < 0) return SYS_ERROR();
  
  info.GetReturnValue().Set(Nan::New<Number>(fd));
}


void init(Handle<Object> target) {
  target->Set(Nan::New<String>("socket"), Nan::New<FunctionTemplate>(Socket)->GetFunction());
  target->Set(Nan::New<String>("fcntl"), Nan::New<FunctionTemplate>(Fcntl)->GetFunction());
  target->Set(Nan::New<String>("connect"), Nan::New<FunctionTemplate>(Connect)->GetFunction());
  target->Set(Nan::New<String>("bind"), Nan::New<FunctionTemplate>(Bind)->GetFunction());
  target->Set(Nan::New<String>("listen"), Nan::New<FunctionTemplate>(Listen)->GetFunction());
  target->Set(Nan::New<String>("accept"), Nan::New<FunctionTemplate>(Accept)->GetFunction());
  target->Set(Nan::New<String>("select"), Nan::New<FunctionTemplate>(Select)->GetFunction());
  target->Set(Nan::New<String>("close"), Nan::New<FunctionTemplate>(Close)->GetFunction());
  target->Set(Nan::New<String>("read"), Nan::New<FunctionTemplate>(Read)->GetFunction());
  target->Set(Nan::New<String>("write"), Nan::New<FunctionTemplate>(Write)->GetFunction());
  target->Set(Nan::New<String>("fork"), Nan::New<FunctionTemplate>(Fork)->GetFunction());
  target->Set(Nan::New<String>("getpid"), Nan::New<FunctionTemplate>(Getpid)->GetFunction());
  target->Set(Nan::New<String>("waitpid"), Nan::New<FunctionTemplate>(Waitpid)->GetFunction());
  target->Set(Nan::New<String>("open"), Nan::New<FunctionTemplate>(Open)->GetFunction());
  
  // Constants
  // socket(2) options
  target->Set(Nan::New<String>("AF_INET"), Nan::New<Number>(AF_INET));
  target->Set(Nan::New<String>("AF_UNIX"), Nan::New<Number>(AF_UNIX));
  target->Set(Nan::New<String>("AF_INET6"), Nan::New<Number>(AF_INET6));
  target->Set(Nan::New<String>("SOCK_STREAM"), Nan::New<Number>(SOCK_STREAM));
  target->Set(Nan::New<String>("SOCK_DGRAM"), Nan::New<Number>(SOCK_DGRAM));
  // fcntl(2) options
  target->Set(Nan::New<String>("F_SETFL"), Nan::New<Number>(F_SETFL));
  target->Set(Nan::New<String>("F_GETFL"), Nan::New<Number>(F_GETFL));
  target->Set(Nan::New<String>("O_NONBLOCK"), Nan::New<Number>(O_NONBLOCK));
  // open(2) flags
  target->Set(Nan::New<String>("O_RDONLY"), Nan::New<Number>(O_RDONLY));
  target->Set(Nan::New<String>("O_WRONLY"), Nan::New<Number>(O_WRONLY));
  target->Set(Nan::New<String>("O_RDWR"), Nan::New<Number>(O_RDWR));
}
NODE_MODULE(syscalls, init)
