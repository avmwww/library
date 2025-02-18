/*
 *
 */

#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "serial.h"

/*
 *
 */
void serial_close(serial_handle fd)
{
	if (fd < 0)
		return;

	close(fd);
}

/*
 *
 */
serial_handle serial_open(const char *path)
{
	int fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);

	if (fd < 0)
		return -1;

	fcntl(fd, F_SETFL, 0);
	ioctl(fd, TCFLSH, TCIOFLUSH);
	return fd;
}

/*
 *
 */
int serial_set_timeout(serial_handle fd, float tmo)
{
	struct termios2 t;

	if (ioctl(fd, TCGETS2, &t) < 0)
		return -1;

	/* timeout in diciseconds for read */
	t.c_cc[VTIME] = tmo * 10;
	return ioctl(fd, TCSETS2, &t);
}

/*
 *
 */
int serial_setup(serial_handle fd, unsigned int baud)
{
	struct termios2 t;

	if (ioctl(fd, TCGETS2, &t) < 0)
		return -1;

	t.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
			| INLCR | IGNCR | ICRNL | IXON);
	t.c_oflag &= ~OPOST;
	t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	t.c_cflag &= ~(CSIZE | PARENB | CRTSCTS);
	t.c_cflag |= CS8 | CREAD | CLOCAL;

	t.c_cflag &= ~(CBAUD | (CBAUD << IBSHIFT));
	t.c_cflag |= BOTHER;

	t.c_ospeed = t.c_ispeed = baud;

	/* Minimum number of characters for noncanonical read */
	t.c_cc[VMIN]  = 0;
	/* Timeout in deciseconds for noncanonical read */
	t.c_cc[VTIME] = 1;

	return ioctl(fd, TCSETS2, &t);
}

/*
 *
 */
int serial_read(serial_handle fd, void *buf, unsigned int len)
{
	char *p = buf;
	int n = 0, rd = 0;

	while (len && (n = read(fd, p, len)) > 0) {
		rd += n;
		p += n;
		len -= n;
	}
	if (n < 0)
		return n;

	return rd;

}

/*
 *
 */
int serial_write(serial_handle fd, const void *buf, unsigned int len)
{
	const char *p = buf;
	int n = 0, wr = 0;

	while (len && (n = write(fd, buf, len)) > 0) {
		wr += n;
		p += n;
		len -= n;
	}
	if (n < 0)
		return n;

	return wr;
}

