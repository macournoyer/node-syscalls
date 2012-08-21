#include <node.h>
#include <v8.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

using namespace v8;

#define TYPE_ERROR(msg) \
    ThrowException(Exception::TypeError(String::New(msg)))

#define SYS_ERROR() \
    ThrowException(Exception::Error(String::New(strerror(errno))))

Handle<Value> Socket(const Arguments& args) {
  HandleScope scope;
  
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return SYS_ERROR();
  
  int sock_flags = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sock_flags, sizeof(sock_flags));
  if (ret < 0) return SYS_ERROR();
  
  return scope.Close(Number::New(fd));
}

Handle<Value> Fcntl(const Arguments& args) {
  if (args.Length() != 3) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
    return TYPE_ERROR("Wrong type of arguments. Expecting numbers");
  }
  
  int fd = args[0]->NumberValue();
  int cmd = args[1]->NumberValue();
  int val = args[2]->NumberValue();
  
  int ret = fcntl(fd, cmd, val);
  if (ret < 0) return SYS_ERROR();
  
  return Undefined();
}

Handle<Value> Connect(const Arguments& args) {
  if (args.Length() != 3) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsString()) {
    return TYPE_ERROR("Wrong type of arguments. Expecting number, number, string");
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
  
  return Undefined();
}

Handle<Value> Bind(const Arguments& args) {
  if (args.Length() != 3) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsString()) {
    return TYPE_ERROR("Wrong type of arguments. Expecting number, number, string");
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
  
  return Undefined();
}

Handle<Value> Listen(const Arguments& args) {
  if (args.Length() != 1) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber()) {
    return TYPE_ERROR("Wrong type of argument. Expecting number");
  }
  
  int fd = args[0]->NumberValue();
  
  int ret = listen(fd, 511);
  if (ret < 0) return SYS_ERROR();
  
  return Undefined();
}

Handle<Value> Accept(const Arguments& args) {
  if (args.Length() != 1) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber()) {
    return TYPE_ERROR("Wrong type of argument. Expecting number");
  }
  
  int fd = args[0]->NumberValue();
  
  struct sockaddr addr;
  socklen_t size = sizeof(addr);
  int cfd = accept(fd, &addr, &size);
  if (cfd < 0) return SYS_ERROR();
  
  HandleScope scope;
  return scope.Close(Number::New(cfd));
}

Handle<Value> Select(const Arguments& args) {
  if (args.Length() != 2) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsArray() || !args[1]->IsArray()) {
    return TYPE_ERROR("Wrong type of arguments. Expecting arrays");
  }
  
  int nfds = 0; // Max fd+1 to watch
  fd_set fds[2];
  
  // Convert args to fd_sets. fds[0] = read fds, fds[1] = write fds.
  unsigned set_i, i;
  for(set_i = 0; set_i < 2; ++set_i) {
    FD_ZERO(&fds[set_i]);
    Handle<Array> array = Handle<Array>::Cast(args[set_i]);
    for(i = 0; i < array->Length(); ++i) {
      int fd = array->Get(i)->NumberValue();
      if (fd + 1 > nfds) nfds = fd + 1;
      FD_SET(fd, &fds[set_i]);
    }
  }
  
  int ret = select(nfds, &fds[0], &fds[1], 0, 0);
  if (ret < 0) return SYS_ERROR();
  
  Local<Array> retfds[2];
  for(set_i = 0; set_i < 2; ++set_i) {
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
  
  HandleScope scope;
  Local<Array> retarray = Array::New(2);
  retarray->Set(0, retfds[0]);
  retarray->Set(1, retfds[1]);
  return scope.Close(retarray);
}

Handle<Value> Close(const Arguments& args) {
  if (args.Length() != 1) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber()) {
    return TYPE_ERROR("Wrong type of argument. Expecting number");
  }
  
  int fd = args[0]->NumberValue();
  
  int ret = close(fd);
  if (ret < 0) return SYS_ERROR();
  
  return Undefined();
}

Handle<Value> Read(const Arguments& args) {
  if (args.Length() != 2) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber() || !args[0]->IsNumber()) {
    return TYPE_ERROR("Wrong type of argument. Expecting number");
  }
  
  int fd = args[0]->NumberValue();
  int nbyte = args[1]->NumberValue();
  char *buf = (char *)malloc(nbyte);
  
  int ret = read(fd, buf, nbyte);
  if (ret < 0) {
    free(buf);
    return SYS_ERROR();
  }
  
  Local<String> str = String::New(buf);
  free(buf);
  
  HandleScope scope;
  return scope.Close(str);
}

Handle<Value> Write(const Arguments& args) {
  if (args.Length() != 2) {
    return TYPE_ERROR("Wrong number of arguments");
  }

  if (!args[0]->IsNumber() || !args[1]->IsString()) {
    return TYPE_ERROR("Wrong type of argument. Expecting number & string");
  }
  
  int fd = args[0]->NumberValue();
  Local<String> str = args[1]->ToString();
  String::AsciiValue buf(str);
  int nbyte = str->Length();
  
  int ret = write(fd, *buf, nbyte);
  if (ret < 0) return SYS_ERROR();
  
  return Undefined();
}

Handle<Value> Fork(const Arguments& args) {
  int ret = fork();
  if (ret < 0) return SYS_ERROR();
  
  HandleScope scope;
  return scope.Close(Number::New(ret));
}

Handle<Value> Getpid(const Arguments& args) {
  HandleScope scope;
  return scope.Close(Number::New(getpid()));
}


void init(Handle<Object> target) {
  target->Set(String::NewSymbol("socket"), FunctionTemplate::New(Socket)->GetFunction());
  target->Set(String::NewSymbol("fcntl"), FunctionTemplate::New(Fcntl)->GetFunction());
  target->Set(String::NewSymbol("connect"), FunctionTemplate::New(Connect)->GetFunction());
  target->Set(String::NewSymbol("bind"), FunctionTemplate::New(Bind)->GetFunction());
  target->Set(String::NewSymbol("listen"), FunctionTemplate::New(Listen)->GetFunction());
  target->Set(String::NewSymbol("accept"), FunctionTemplate::New(Accept)->GetFunction());
  target->Set(String::NewSymbol("select"), FunctionTemplate::New(Select)->GetFunction());
  target->Set(String::NewSymbol("close"), FunctionTemplate::New(Close)->GetFunction());
  target->Set(String::NewSymbol("read"), FunctionTemplate::New(Read)->GetFunction());
  target->Set(String::NewSymbol("write"), FunctionTemplate::New(Write)->GetFunction());
  target->Set(String::NewSymbol("fork"), FunctionTemplate::New(Fork)->GetFunction());
  target->Set(String::NewSymbol("getpid"), FunctionTemplate::New(Getpid)->GetFunction());
  
  // Constants
  target->Set(String::NewSymbol("F_SETFL"), Number::New(F_SETFL));
  target->Set(String::NewSymbol("O_NONBLOCK"), Number::New(O_NONBLOCK));
}
NODE_MODULE(syscalls, init)
