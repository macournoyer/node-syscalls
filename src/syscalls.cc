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

#define SYS_ERROR() NanThrowError(strerror(errno))

NAN_METHOD(Socket) {
  NanScope();

  if (args.Length() != 3) {
    return NanThrowTypeError("Wrong number of arguments. Expecting domain, type, protocol.");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
    return NanThrowTypeError("Wrong type of arguments. Expecting numbers");
  }
  
  int domain = args[0]->NumberValue();
  int type = args[1]->NumberValue();
  int protocol = args[2]->NumberValue();
  
  int fd = socket(domain, type, protocol);
  if (fd < 0) return SYS_ERROR();
  
  int sock_flags = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sock_flags, sizeof(sock_flags));
  if (ret < 0) return SYS_ERROR();
  
  NanReturnValue(Number::New(fd));
}

NAN_METHOD(Fcntl) {
  NanScope();

  if (args.Length() != 3) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD, command, value.");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
    return NanThrowTypeError("Wrong type of arguments. Expecting numbers");
  }
  
  int fd = args[0]->NumberValue();
  int cmd = args[1]->NumberValue();
  int val = args[2]->NumberValue();
  
  int ret = fcntl(fd, cmd, val);
  if (ret < 0) return SYS_ERROR();
  
  NanReturnValue(Number::New(ret));
}

NAN_METHOD(Connect) {
  NanScope();

  if (args.Length() != 3) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD, port, address.");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsString()) {
    return NanThrowTypeError("Wrong type of arguments. Expecting number, number, string");
  }
  
  int fd = args[0]->NumberValue();
  int port = args[1]->NumberValue();
  Local<String> addrString = args[2]->ToString();
  String::AsciiValue addr_str(addrString);
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(*addr_str);
  int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0 && errno != EINPROGRESS) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Bind) {
  NanScope();

  if (args.Length() != 3) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD, port, address.");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsString()) {
    return NanThrowTypeError("Wrong type of arguments. Expecting number, number, string");
  }
  
  int fd = args[0]->NumberValue();
  int port = args[1]->NumberValue();
  Local<String> addrString = args[2]->ToString();
  String::AsciiValue addr_str(addrString);
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(*addr_str);
  int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Listen) {
  NanScope();

  if (args.Length() != 2) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD, backlog");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber()) {
    return NanThrowTypeError("Wrong type of argument. Expecting number, number");
  }
  
  int fd = args[0]->NumberValue();
  int backlog = args[1]->NumberValue();
  
  int ret = listen(fd, backlog);
  if (ret < 0) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Accept) {
  NanScope();

  if (args.Length() != 1) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD.");
  }

  if (!args[0]->IsNumber()) {
    return NanThrowTypeError("Wrong type of argument. Expecting number");
  }
  
  int fd = args[0]->NumberValue();
  
  struct sockaddr addr;
  socklen_t size = sizeof(addr);
  int cfd = accept(fd, &addr, &size);
  if (cfd < 0) return SYS_ERROR();
  
  NanReturnValue(Number::New(cfd));
}

NAN_METHOD(Select) {
  NanScope();

  if (args.Length() < 3 || args.Length() > 4) {
    return NanThrowTypeError("Wrong number of arguments. Expecting readables, writables, exceptionals[, timeout].");
  }

  if (!args[0]->IsArray() || !args[1]->IsArray() || !args[2]->IsArray() ||
      (args.Length() == 4 && !args[3]->IsNumber())) {
    return NanThrowTypeError("Wrong type of arguments. Expecting array, array, array[, number]");
  }
  
  int nfds = 0; // Max fd+1 to watch
  fd_set fds[3];
  
  // Convert args to fd_sets. fds[0] = read fds, fds[1] = write fds, fds[2] = error fds.
  unsigned set_i, i;
  for(set_i = 0; set_i < 3; ++set_i) {
    FD_ZERO(&fds[set_i]);
    Handle<Array> array = Handle<Array>::Cast(args[set_i]);
    for(i = 0; i < array->Length(); ++i) {
      int fd = array->Get(i)->NumberValue();
      if (fd + 1 > nfds) nfds = fd + 1;
      FD_SET(fd, &fds[set_i]);
    }
  }
  
  // Set the timeout (in sec)
  struct timeval timeout, *timeoutp = 0;
  if (args.Length() == 4) {
    timeout.tv_usec = 0;
    timeout.tv_sec = args[3]->NumberValue();
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
    retfds[set_i] = Array::New();
    Handle<Array> array = Handle<Array>::Cast(args[set_i]);
    for(i = 0; i < array->Length(); ++i) {
      int fd = array->Get(i)->NumberValue();
      if (FD_ISSET(fd, &fds[set_i])) {
        retfds[set_i]->Set(ret_i++, Number::New(fd));
      }
    }
  }
  
  Local<Array> retarray = Array::New(3);
  retarray->Set(0, retfds[0]);
  retarray->Set(1, retfds[1]);
  retarray->Set(2, retfds[2]);

  NanReturnValue(retarray);
}

NAN_METHOD(Close) {
  NanScope();

  if (args.Length() != 1) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD.");
  }

  if (!args[0]->IsNumber()) {
    return NanThrowTypeError("Wrong type of argument. Expecting number");
  }
  
  int fd = args[0]->NumberValue();
  
  int ret = close(fd);
  if (ret < 0) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Read) {
  NanScope();

  if (args.Length() != 2) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD, number of bytes.");
  }

  if (!args[0]->IsNumber() || !args[0]->IsNumber()) {
    return NanThrowTypeError("Wrong type of argument. Expecting number, number.");
  }
  
  int fd = args[0]->NumberValue();
  int nbyte = args[1]->NumberValue();
  char *buf = (char *)malloc(nbyte);
  
  int ret = read(fd, buf, nbyte);
  if (ret < 0) {
    free(buf);
    return SYS_ERROR();
  }
  
  Local<String> str = String::New(buf, ret);
  free(buf);
  
  NanReturnValue(str);
}

NAN_METHOD(Write) {
  NanScope();

  if (args.Length() < 2 || args.Length() > 3) {
    return NanThrowTypeError("Wrong number of arguments. Expecting FD, data.");
  }

  if (!args[0]->IsNumber() || !args[1]->IsString()) {
    return NanThrowTypeError("Wrong type of argument. Expecting number, string.");
  }
  
  int fd = args[0]->NumberValue();
  Local<String> str = args[1]->ToString();
  String::AsciiValue buf(str);
  int nbyte = str->Length();
  
  int ret = write(fd, *buf, nbyte);
  if (ret < 0) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Fork) {
  NanScope();

  int ret = fork();
  if (ret < 0) return SYS_ERROR();
  
  NanReturnValue(Number::New(ret));
}

NAN_METHOD(Getpid) {
  NanScope();

  NanReturnValue(Number::New(getpid()));
}

NAN_METHOD(Waitpid) {
  NanScope();

  pid_t pid = -1; // Default to waiting for all child processes.
  if (args.Length() >= 1) {
    pid = args[0]->NumberValue();
  }
  
  int options = 0;
  if (args.Length() >= 2) {
    options = args[1]->NumberValue();
  }
    
  int status;
  
  int ret = waitpid(pid, &status, options);
  if (ret < 0) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Open) {
  NanScope();

  if (args.Length() != 2) {
    return NanThrowTypeError("Wrong number of arguments. Expecting path, flags.");
  }

  if (!args[0]->IsString() || !args[1]->IsNumber()) {
    return NanThrowTypeError("Wrong type of argument. Expecting string, number.");
  }
  
  Local<String> str = args[0]->ToString();
  String::AsciiValue path(str);
  int flags = args[1]->NumberValue();
  
  int fd = open(*path, flags);
  if (fd < 0) return SYS_ERROR();
  
  NanReturnValue(Number::New(fd));
}


void init(Handle<Object> target) {
  target->Set(NanSymbol("socket"), FunctionTemplate::New(Socket)->GetFunction());
  target->Set(NanSymbol("fcntl"), FunctionTemplate::New(Fcntl)->GetFunction());
  target->Set(NanSymbol("connect"), FunctionTemplate::New(Connect)->GetFunction());
  target->Set(NanSymbol("bind"), FunctionTemplate::New(Bind)->GetFunction());
  target->Set(NanSymbol("listen"), FunctionTemplate::New(Listen)->GetFunction());
  target->Set(NanSymbol("accept"), FunctionTemplate::New(Accept)->GetFunction());
  target->Set(NanSymbol("select"), FunctionTemplate::New(Select)->GetFunction());
  target->Set(NanSymbol("close"), FunctionTemplate::New(Close)->GetFunction());
  target->Set(NanSymbol("read"), FunctionTemplate::New(Read)->GetFunction());
  target->Set(NanSymbol("write"), FunctionTemplate::New(Write)->GetFunction());
  target->Set(NanSymbol("fork"), FunctionTemplate::New(Fork)->GetFunction());
  target->Set(NanSymbol("getpid"), FunctionTemplate::New(Getpid)->GetFunction());
  target->Set(NanSymbol("waitpid"), FunctionTemplate::New(Waitpid)->GetFunction());
  target->Set(NanSymbol("open"), FunctionTemplate::New(Open)->GetFunction());
  
  // Constants
  // socket(2) options
  target->Set(NanSymbol("AF_INET"), Number::New(AF_INET));
  target->Set(NanSymbol("AF_UNIX"), Number::New(AF_UNIX));
  target->Set(NanSymbol("AF_INET6"), Number::New(AF_INET6));
  target->Set(NanSymbol("SOCK_STREAM"), Number::New(SOCK_STREAM));
  target->Set(NanSymbol("SOCK_DGRAM"), Number::New(SOCK_DGRAM));
  // fcntl(2) options
  target->Set(NanSymbol("F_SETFL"), Number::New(F_SETFL));
  target->Set(NanSymbol("F_GETFL"), Number::New(F_GETFL));
  target->Set(NanSymbol("O_NONBLOCK"), Number::New(O_NONBLOCK));
  // open(2) flags
  target->Set(NanSymbol("O_RDONLY"), Number::New(O_RDONLY));
  target->Set(NanSymbol("O_WRONLY"), Number::New(O_WRONLY));
  target->Set(NanSymbol("O_RDWR"), Number::New(O_RDWR));
}
NODE_MODULE(syscalls, init)
