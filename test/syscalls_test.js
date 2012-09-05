var assert = require("assert");
var syscalls = require("../build/Release/syscalls");

describe('syscalls', function() {
  var fd = syscalls.socket(syscalls.AF_INET, syscalls.SOCK_STREAM, 0);
  
  it('should create socket', function() {
    assert(fd > 2);
  });
  
  it('should bind', function() {
    syscalls.bind(fd, 3000, "0.0.0.0");
  });
  
  it('should listen', function() {
    syscalls.listen(fd, 100);
  });
  
  it('should select', function() {
    var fds = syscalls.select([], [], [], 1);
    assert.equal(3, fds.length);
    assert.equal(0, fds[0].length);
    assert.equal(0, fds[1].length);
    assert.equal(0, fds[2].length);
  });
  
  it('should accept', function() {
    syscalls.fcntl(fd, syscalls.F_SETFL, syscalls.O_NONBLOCK);
    try {
      syscalls.accept(fd);
      assert.fail("Should throw error");
    }
    catch (e) {
      assert.equal("Resource temporarily unavailable", e.message);
    }
  });
  
  it('close', function() {
    syscalls.close(fd);
  });
});