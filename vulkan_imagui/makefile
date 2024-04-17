NAME      := a.out

UTILS     := ../utils/
INCLCUDES := -I${UTILS} -I${UTILS}/ap -I${UTILS}/vulkan -I${UTILS}/generic -I.
INCLCUDES += -I../imgui -I../imgui/backends
LIBS      := -lpthread -ldl -lglfw -lcurl -lvulkan

MACHINE_INDEPENDENT := $(shell g++ -lMachineIndependent 2>&1)
ifneq (,$(findstring cannot find -lMachineIndependent,$(MACHINE_INDEPENDENT)))
	GLSL_ADDITIONAL_LIB :=
else
	GLSL_ADDITIONAL_LIB := -lMachineIndependent -lGenericCodeGen
endif

LIBS      += -Wl,--start-group
LIBS 	  += -lglslang -lOGLCompiler -lSPIRV -lOSDependent 
LIBS 	  += -lSPIRV-Tools -lSPIRV-Tools-opt -lSPVRemapper -lHLSL 
LIBS 	  += ${GLSL_ADDITIONAL_LIB}
LIBS 	  += -Wl,--end-group
LIBS      += -lpthread

SRCS      := $(wildcard ./*.cpp)
SRCS      += $(wildcard ../imgui/*.cpp)
SRCS 	  += ../imgui/backends/imgui_impl_vulkan.cpp
SRCS 	  += ../imgui/backends/imgui_impl_glfw.cpp
SRCS      += $(wildcard ${UTILS}/*.cpp)
OBJS      := $(SRCS:.cpp=.o)
DEPS      := $(SRCS:.cpp=.d)
CXX 	  := g++-11
CXX_FLAGS := -std=c++2a -g -export-dynamic
CXX_FLAGS += -Wno-format-security

all: ${NAME}

${NAME}: ${DEPS} ${OBJS}
	${CXX} ${CXX_FLAGS} ${INCLCUDES} ${OBJS} ${LIBS} -o $@

${DEPS}: makefile
${OBJS}: makefile

${DEPS}:%.d:%.cpp
	${CXX} -c ${CXX_FLAGS} ${INCLCUDES} -MM $< -MF $@

include ${DEPS}

${OBJS}:%.o:%.cpp
	${CXX} -c ${CXX_FLAGS} ${INCLCUDES} $< -o $@

clean:
	rm -f ${OBJS}
	rm -f ${DEPS}
	rm -f ${NAME}