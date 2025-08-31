import os
# f = os.environ['PATH']

# f = os.getpid()
# f = os.getppid()
# print(f)

f1 = os.getuid()
f2 = os.getgid()
f3 = os.geteuid()   # 유효 사용자 ID
f4 = os.getegid()   # 유효 그룹 ID

print(f1, f2, f3, f4)