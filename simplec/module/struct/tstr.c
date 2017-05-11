﻿#include <tstr.h>

//-----------------------------------字符串相关的协助API -------------------------------

//
// 采用jshash算法,计算字符串返回后的hash值,碰撞率为80%
// str		: 字符串内容
// return	: 返回计算后的hash值
//
unsigned 
tstr_hash(const char * str) {
	unsigned i, h, sp;
	if (!str || !*str) 
		return 1;

	h = (unsigned)strlen(str);
	sp = (h >> 5) + 1;
	for (i = h; i >= sp; i -= sp)
		h ^= (h << 5) + (h >> 2) + (unsigned char)str[i - 1];

	return h ? h : 1;
}

//
// 字符串不区分大小写比较函数
// ls		: 左串
// rs		: 右串
// return	: ls > rs 返回 > 0; ... < 0; ... =0
//
int 
tstr_icmp(const char * ls, const char * rs) {
	int l, r;
	if (!ls || !rs) 
		return (int)(ls - rs);

	do {
		if ((l = *ls++) >= 'a' && l <= 'z')
			l -= 'a' - 'A';
		if ((r = *rs++) >= 'a' && r <= 'z')
			r -= 'a' - 'A';
	} while (l && l == r);

	return l - r;
}

//
// 字符串拷贝函数, 需要自己free
// str		: 待拷贝的串
// return	: 返回拷贝后的串
//
char * 
tstr_dup(const char * str) {
	size_t len;
	char * nstr;
	if (NULL == str) 
		return NULL;

	len = strlen(str) + 1;
	nstr = malloc(sizeof(char) * len);
	//
	// 补充一下, 关于malloc的写法, 说不尽道不完. 
	// 这里采用 日志 + exit, 这种未定义行为. 方便收集错误日志和监测大内存申请失败情况.
	//
	if (NULL == nstr)
		CERR_EXIT("malloc len = %zu is empty!", len);

	return memcpy(nstr, str, len);
}

//
// 简单的文件读取类,会读取完毕这个文件内容返回,失败返回NULL.
// path		: 文件路径
// return	: 创建好的字符串内容, 返回NULL表示读取失败
//
tstr_t 
tstr_freadend(const char * path) {
	tstr_t tstr;
	char buf[BUFSIZ];
	size_t rn;
	FILE * txt = fopen(path, "rb");
	if (NULL == txt) {
		CERR("fopen r %s is error!", path);
		return NULL;
	}

	// 分配内存
	tstr = tstr_create(NULL);

	// 读取文件内容
	do {
		rn = fread(buf, sizeof(char), BUFSIZ, txt);
		if (rn < 0)
			break;
		// 保存构建好的数据
		tstr_appendn(tstr, buf, rn);
	} while (rn == BUFSIZ);

	fclose(txt);
	// 最终错误检查
	if (rn < 0) {
		tstr_delete(tstr);
		CERR("fread is error! path = %s. rn = %zu.", path, rn);
		return NULL;
	}

	// 继续构建数据, 最后一行补充一个\0
	tstr_cstr(tstr);

	return tstr;
}

static flag_e _tstr_fwrite(const char * path, const char * str, const char * mode) {
	FILE * txt;
	if (!path || !*path || !str) {
		CERR("check !path || !*path || !str'!!!");
		return Error_Param;
	}

	// 打开文件, 写入消息, 关闭文件
	if ((txt = fopen(path, mode)) == NULL) {
		CERR("fopen mode = '%s', path = '%s' error!", mode, path);
		return Error_Fd;
	}
	fputs(str, txt);
	fclose(txt);

	return Success_Base;
}

//
// 将c串str覆盖写入到path路径的文件中
// path		: 文件路径
// str		: c的串内容
// return	: Success_Base | Error_Param | Error_Fd
//
inline flag_e 
tstr_fwrites(const char * path, const char * str) {
	return _tstr_fwrite(path, str, "wb");
}

//
// 将c串str写入到path路径的文件中末尾
// path		: 文件路径
// str		: c的串内容
// return	: Success_Base | Error_Param | Error_Fd
//
inline flag_e 
tstr_fappends(const char * path, const char * str) {
	return _tstr_fwrite(path, str, "ab");
}

//-----------------------------------字符串相关的核心API -------------------------------

//
// tstr_t 创建函数, 会根据c的tstr串创建一个 tstr_t结构的字符串
// str		: 待创建的字符串
// return	: 返回创建好的字符串,内存不足会打印日志退出程序
//
tstr_t 
tstr_create(const char * str) {
	tstr_t tstr = calloc(1, sizeof(struct tstr));
	if (NULL == tstr)
		CERR_EXIT("malloc sizeof struct tstr is error!");
	if(str)
		tstr_appends(tstr, str);
	return tstr;
}

//
// tstr_t 释放函数, 不处理多释放问题
// tstr		: 待释放的串结构
//
inline void 
tstr_delete(tstr_t tstr) {
	free(tstr->str);
	free(tstr);
}

// 文本字符串创建的初始化大小
#define _INT_TSTRING	(32)

// 串内存检查, 不够就重新构建内存
static void _tstr_realloc(tstr_t tstr, size_t len) {
	char * nstr;
	size_t cap = tstr->cap;
	if (len < cap)
		return;

	// 开始分配内存
	for (cap = cap < _INT_TSTRING ? _INT_TSTRING : cap; cap < len; cap <<= 1)
		;
	if ((nstr = realloc(tstr->str, cap)) == NULL) {
		tstr_delete(tstr);
		CERR_EXIT("realloc cap = %zu empty!!!", cap);
	}

	// 重新内联内存
	tstr->str = nstr;
	tstr->cap = cap;
}

//
// 向tstr_t串结构中添加字符等, 内存分配失败内部会自己处理
// c		: 单个添加的char
// str		: 添加的c串
// sz		: 添加串的长度
//
inline void 
tstr_appendc(tstr_t tstr, int c) {
	// 这类函数不做安全检查, 为了性能
	_tstr_realloc(tstr, ++tstr->len);
	tstr->str[tstr->len - 1] = c;
}

void 
tstr_appends(tstr_t tstr, const char * str) {
	size_t len;
	if (!tstr || !str) {
		CERR("check '!tstr || !str' param is error!");
		return;
	}

	len = strlen(str);
	if(len > 0)
		tstr_appendn(tstr, str, len);
	tstr_cstr(tstr);
}

inline void 
tstr_appendn(tstr_t tstr, const char * str, size_t sz) {
	_tstr_realloc(tstr, tstr->len += sz);
	memcpy(tstr->str + tstr->len - sz, str, sz);
}

//
// 得到一个精简的c的串, 需要自己事后free
// tstr		: tstr_t 串
// return	: 返回创建好的c串
//
char * 
tstr_dupstr(tstr_t tstr) {
	size_t len;
	char * str;
	if (!tstr || tstr->len < 1)
		return NULL;

	// 构造内存, 最终返回结果
	len = tstr->len + !!tstr->str[tstr->len - 1];
	str = malloc(len * sizeof(char));
	if (NULL == str)
		CERR_EXIT("malloc len = %zu is error!", len);
	
	memcpy(str, tstr->str, len - 1);
	str[len - 1] = '\0';

	return str;
}

//
// 通过cstr_t串得到一个c的串以'\0'结尾
// tstr		: tstr_t 串
// return	: 返回构建好的c的串, 内存地址tstr->str
//
char * 
tstr_cstr(tstr_t tstr) {
	// 本质是检查最后一个字符是否为 '\0'
	if (tstr->len < 1 || tstr->str[tstr->len - 1]) {
		_tstr_realloc(tstr, tstr->len + 1);
		tstr->str[tstr->len] = '\0';
	}

	return tstr->str;
}
