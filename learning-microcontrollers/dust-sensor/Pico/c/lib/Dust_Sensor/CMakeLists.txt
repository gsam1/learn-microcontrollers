# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_Dust_Sensor_SRCS 变量
aux_source_directory(. DIR_Dust_Sensor_SRCS)

include_directories(../Config)

# 生成链接库
add_library(Dust_Sensor ${DIR_Dust_Sensor_SRCS})
target_link_libraries(Dust_Sensor PUBLIC Config)