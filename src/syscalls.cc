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
  
  NanReturnValue(NanNew<Number>(fd));
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
  
  NanReturnValue(NanNew<Number>(ret));
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
  NanAsciiString addr_str(args[2]);
  
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
  NanAsciiString addr_str(args[2]);
  
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
  int cfd;
  while ((cfd = accept(fd, &addr, &size)) == -1 && errno == EINTR) continue;
  if (cfd < 0) return SYS_ERROR();
  
  NanReturnValue(NanNew<Number>(cfd));
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
    retfds[set_i] = NanNew<Array>();
    Handle<Array> array = Handle<Array>::Cast(args[set_i]);
    for(i = 0; i < array->Length(); ++i) {
      int fd = array->Get(i)->NumberValue();
      if (FD_ISSET(fd, &fds[set_i])) {
        retfds[set_i]->Set(ret_i++, NanNew<Number>(fd));
      }
    }
  }
  
  Local<Array> retarray = NanNew<Array>(3);
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
  
  Local<String> str = NanNew<String>(buf, ret);
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
  NanAsciiString buf(args[1]);
  int nbyte = str->Length();
  
  int ret = write(fd, *buf, nbyte);
  if (ret < 0) return SYS_ERROR();
  
  NanReturnUndefined();
}

NAN_METHOD(Fork) {
  NanScope();

  int ret = fork();
  if (ret < 0) return SYS_ERROR();
  
  NanReturnValue(NanNew<Number>(ret));
}

NAN_METHOD(Getpid) {
  NanScope();

  NanReturnValue(NanNew<Number>(getpid()));
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
  
  NanAsciiString path(args[0]);
  int flags = args[1]->NumberValue();
  
  int fd = open(*path, flags);
  if (fd < 0) return SYS_ERROR();
  
  NanReturnValue(NanNew<Number>(fd));
}


void init(Handle<Object> target) {
  target->Set(NanNew<String>("socket"), NanNew<FunctionTemplate>(Socket)->GetFunction());
  target->Set(NanNew<String>("fcntl"), NanNew<FunctionTemplate>(Fcntl)->GetFunction());
  target->Set(NanNew<String>("connect"), NanNew<FunctionTemplate>(Connect)->GetFunction());
  target->Set(NanNew<String>("bind"), NanNew<FunctionTemplate>(Bind)->GetFunction());
  target->Set(NanNew<String>("listen"), NanNew<FunctionTemplate>(Listen)->GetFunction());
  target->Set(NanNew<String>("accept"), NanNew<FunctionTemplate>(Accept)->GetFunction());
  target->Set(NanNew<String>("select"), NanNew<FunctionTemplate>(Select)->GetFunction());
  target->Set(NanNew<String>("close"), NanNew<FunctionTemplate>(Close)->GetFunction());
  target->Set(NanNew<String>("read"), NanNew<FunctionTemplate>(Read)->GetFunction());
  target->Set(NanNew<String>("write"), NanNew<FunctionTemplate>(Write)->GetFunction());
  target->Set(NanNew<String>("fork"), NanNew<FunctionTemplate>(Fork)->GetFunction());
  target->Set(NanNew<String>("getpid"), NanNew<FunctionTemplate>(Getpid)->GetFunction());
  target->Set(NanNew<String>("waitpid"), NanNew<FunctionTemplate>(Waitpid)->GetFunction());
  target->Set(NanNew<String>("open"), NanNew<FunctionTemplate>(Open)->GetFunction());
  
  // Constants
  // socket(2) options
  target->Set(NanNew<String>("AF_INET"), NanNew<Number>(AF_INET));
  target->Set(NanNew<String>("AF_UNIX"), NanNew<Number>(AF_UNIX));
  target->Set(NanNew<String>("AF_INET6"), NanNew<Number>(AF_INET6));
  target->Set(NanNew<String>("SOCK_STREAM"), NanNew<Number>(SOCK_STREAM));
  target->Set(NanNew<String>("SOCK_DGRAM"), NanNew<Number>(SOCK_DGRAM));
  // fcntl(2) options
  target->Set(NanNew<String>("F_SETFL"), NanNew<Number>(F_SETFL));
  target->Set(NanNew<String>("F_GETFL"), NanNew<Number>(F_GETFL));
  target->Set(NanNew<String>("O_NONBLOCK"), NanNew<Number>(O_NONBLOCK));
  // open(2) flags
  target->Set(NanNew<String>("O_RDONLY"), NanNew<Number>(O_RDONLY));
  target->Set(NanNew<String>("O_WRONLY"), NanNew<Number>(O_WRONLY));
  target->Set(NanNew<String>("O_RDWR"), NanNew<Number>(O_RDWR));
}
NODE_MODULE(syscalls, init)
