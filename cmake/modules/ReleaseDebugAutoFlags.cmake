## debug / release autoManagement



set(CMAKE_C_FLAGS_RELEASE  "-O2")
set(CMAKE_C_FLAGS_DEBUG  "-g -Wall -Wextra -pedantic -fstack-protector-all")
set(CMAKE_C_FLAGS_RELWITHDEBINFO " -Wall -g -O2")


set(CMAKE_CXX_FLAGS_RELEASE  "-O2")
set(CMAKE_CXX_FLAGS_DEBUG  "-g -Wall -Wextra  -fstack-protector-all ")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -Wall -g -O2")
