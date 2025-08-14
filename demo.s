# demo program for demo.bin, ends with a zero word (halt)
addi x1, x0, 5         # 0x0000: 00500093
addi x2, x0, 10        # 0x0004: 00a00113
add  x3, x1, x2        # 0x0008: 002081b3
sub  x4, x2, x1        # 0x000c: 40110233
and  x5, x1, x2        # 0x0010: 0020f2b3
or   x6, x1, x2        # 0x0014: 0020e333
xor  x7, x1, x2        # 0x0018: 0020c3b3
slti x8, x1, 7         # 0x001c: 0070a413
sltiu x9, x1, 3        # 0x0020: 0030b493
xori x10, x1, -1       # 0x0024: fff0c513
ori  x11, x1, 16       # 0x0028: 0100e593
andi x12, x2, 6        # 0x002c: 00617613
lui  x13, 0x12345      # 0x0030: 123456b7
auipc x14, 1           # 0x0034: 00001717
addi x15, x0, 256      # 0x0038: 10000793
sw   x3, 0(x15)        # 0x003c: 0037a023
addi x0, x0, 1         # 0x0040: 00100013
