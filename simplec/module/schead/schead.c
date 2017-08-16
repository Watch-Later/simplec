#include <schead.h>

#if defined(__GUNC__)

//
// getch - �����õ��û������һ���ַ�, linuxʵ��
// return	: ���صõ��ַ�
//
inline int 
getch(void) {
	int cr;
	struct termios nts, ots;
	if (tcgetattr(0, &ots) < 0) // �õ���ǰ�ն�(0��ʾ��׼����)������
		return EOF;

	nts = ots;
	cfmakeraw(&nts); // �����ն�ΪRawԭʼģʽ����ģʽ�����е������������ֽ�Ϊ��λ������
	if (tcsetattr(0, TCSANOW, &nts) < 0) // �����ϸ���֮�������
		return EOF;

	cr = getchar();
	if (tcsetattr(0, TCSANOW, &ots) < 0) // ���û�ԭ���ϵ�ģʽ
		return EOF;
	return cr;
}

#endif

//
// sh_isbe - �ж��Ǵ������С����,����򷵻�true
// sh_hton - ���������ֽ�����ת��'С��'�����ֽ�
// sh_ntoh - ��'С��'�������ֽ���ֵת�ɱ�����ֵ
//
inline bool 
sh_isbe(void) {
	union { uint16_t i; uint8_t c; } _u = { 1 };
	return 0 == _u.c;
}

inline uint32_t 
sh_hton(uint32_t x) {
	if (sh_isbe()) {
		uint8_t t;
		union { uint32_t i; uint8_t s[sizeof(uint32_t)]; } u = { x };
		t = u.s[0], u.s[0] = u.s[sizeof(u) - 1], u.s[sizeof(u) - 1] = t;
		t = u.s[1], u.s[1] = u.s[sizeof(u) - 1 - 1], u.s[sizeof(u) - 1 - 1] = t;
		return u.i;
	}
	return x;
}

inline uint32_t 
sh_ntoh(uint32_t x) {
	return sh_hton(x);
}