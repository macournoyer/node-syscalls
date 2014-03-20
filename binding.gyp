{
  "targets": [
    {
      "target_name": "syscalls",
      "sources": [ "src/syscalls.cc" ],
      "include_dirs" : [
          "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}