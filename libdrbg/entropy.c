/*
 *  Copyright (C) 2022 - This file is part of libdrbg project
 *
 *  Author:       Ryad BENADJILA <ryad.benadjila@ssi.gouv.fr>
 *  Contributor:  Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */

#include "entropy.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>

#include <stdio.h>
#include <termios.h>
#include <sys/select.h>
#include <errno.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define SERIAL_BAUDRATE 460800 // 串口波特率
#define READ_BUF_SIZE 1024	   // 读取缓冲区大小

int serial_init(const char *port, int baudrate)
{
	int fd = open(port, O_RDWR | O_NOCTTY); // 打开串口设备文件
	if (fd == -1)
	{
		perror("open_port: Unable to open serial port"); // 输出错误信息
		return -1;
	}

	struct termios tty;
	memset(&tty, 0, sizeof(tty));
	if (tcgetattr(fd, &tty) != 0)
	{ // 获取串口属性
		perror("tcgetattr");
		close(fd);
		return -1;
	}

	cfsetispeed(&tty, baudrate); // 设置输入波特率
	cfsetospeed(&tty, baudrate); // 设置输出波特率

	tty.c_cflag &= ~PARENB; // 禁用奇偶校验
	tty.c_cflag &= ~CSTOPB; // 设置停止位为1
	tty.c_cflag &= ~CSIZE;	// 清除数据位设置
	tty.c_cflag |= CS8;		// 设置数据位为8位
	// tty.c_cflag &= ~CRTSCTS; // 禁用硬件流控
	tty.c_cflag |= CREAD | CLOCAL; // 启用接收器，本地连接

	tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 设置输入模式
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);			// 禁用软件流控
	tty.c_oflag &= ~OPOST;							// 设置输出模式

	tty.c_cc[VMIN] = 1;	 // 读取一个字符
	tty.c_cc[VTIME] = 0; // 无超时

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{ // 设置串口属性
		perror("tcsetattr");
		close(fd);
		return -1;
	}

	tcflush(fd, TCIFLUSH); // 刷新输入缓冲区

	return fd;
}

/*
 * Copy file content to buffer. Return 0 on success, i.e. if the request
 * size has been read and copied to buffer and -1 otherwise.
 */
static int fimport(uint8_t *buf, uint32_t buflen, const char *path)
{
	uint32_t rem = buflen, copied = 0;
	ssize_t ret;
	int fd;

	if ((buf == NULL) || (path == NULL))
	{
		ret = -1;
		goto err;
	}

	fd = serial_init(SERIAL_PORT, SERIAL_BAUDRATE); // 初始化串口
	if (fd == -1)
	{
		fprintf(stderr, "Failed to initialize serial port.\n");
		return 1;
	}

	// 写入数据到串口
	uint8_t message[2] = {0x4F, 0x52};
	if (write(fd, message, sizeof(message)) != sizeof(message))
	{
		perror("write");
		return -1;
		goto close_fd;
	}

	// 读取数据
	while (rem)
	{
		ret = (int)read(fd, buf + copied, rem);
		if (ret <= 0)
		{
			break;
		}
		else
		{
			rem = (uint32_t)(rem - ret);
			copied = (uint32_t)(copied + ret);
		}
	}

close_fd:
	if (close(fd))
	{
		printf("Unable to close input file %s\n", path);
		ret = -1;
	}

	ret = (copied == buflen) ? 0 : -1;

err:
	return (int)ret;
}

static int _get_entropy_input_from_os(uint8_t *buf, uint32_t len)
{
	int ret;

	ret = fimport(buf, len, "/dev/ttyACM0");

	return ret;
}

#define ENTROPY_BUFF_LEN 1024

/* The entropy */
typedef struct
{
	uint8_t entropy_buff[ENTROPY_BUFF_LEN];
	uint32_t entropy_buff_pos;
	uint32_t entropy_buff_len;
} entropy_pool;

static bool curr_entropy_pool_init = false;
static entropy_pool curr_entropy_pool;

int get_entropy_input(uint8_t **buf, uint32_t len, bool prediction_resistance)
{
	int ret = -1;

	/* Avoid unused parameter warnings */
	(void)prediction_resistance;

	if (curr_entropy_pool_init == false)
	{
		/* Initialize our entropy pool */
		memset(curr_entropy_pool.entropy_buff, 0, sizeof(curr_entropy_pool.entropy_buff));
		curr_entropy_pool.entropy_buff_pos = curr_entropy_pool.entropy_buff_len = 0;

		curr_entropy_pool_init = true;
	}

	/* Sanity check */
	if (buf == NULL)
	{
		goto err;
	}

	(*buf) = NULL;

	/* If we ask for more than the size of our entropy pool, return an error ... */
	if (len > sizeof(curr_entropy_pool.entropy_buff))
	{
		goto err;
	}
	else if (len <= curr_entropy_pool.entropy_buff_len)
	{
		(*buf) = (curr_entropy_pool.entropy_buff + curr_entropy_pool.entropy_buff_pos);
		/* Remove the consumed data */
		curr_entropy_pool.entropy_buff_pos += len;
		curr_entropy_pool.entropy_buff_len -= len;
	}
	else
	{
		/* We do not have enough remaining data, reset and ask for maximum */
		ret = _get_entropy_input_from_os(curr_entropy_pool.entropy_buff, sizeof(curr_entropy_pool.entropy_buff));
		if (ret)
		{
			goto err;
		}
		curr_entropy_pool.entropy_buff_pos = 0;
		curr_entropy_pool.entropy_buff_len = sizeof(curr_entropy_pool.entropy_buff);
		(*buf) = curr_entropy_pool.entropy_buff;
	}

	/* Sanity checks */
	if (curr_entropy_pool.entropy_buff_pos > sizeof(curr_entropy_pool.entropy_buff))
	{
		goto err;
	}
	if (curr_entropy_pool.entropy_buff_len > sizeof(curr_entropy_pool.entropy_buff))
	{
		goto err;
	}

	ret = 0;

err:
	if (ret && (buf != NULL))
	{
		(*buf) = NULL;
	}
	return ret;
}

int clear_entropy_input(uint8_t *buf)
{
	int ret = -1;
	uint8_t *buf_max = (curr_entropy_pool.entropy_buff + curr_entropy_pool.entropy_buff_pos);

	/* Sanity check */
	if ((buf < curr_entropy_pool.entropy_buff) || (buf > buf_max))
	{
		goto err;
	}

	/* Clean the buffer until pos */
	memset(curr_entropy_pool.entropy_buff, 0, curr_entropy_pool.entropy_buff_pos);

	ret = 0;
err:
	return ret;
}