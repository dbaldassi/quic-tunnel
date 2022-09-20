#ifndef CALLBACK_H
#define CALLBACK_H

#ifdef __cplusplus
extern "C" {
#endif

	typedef void* wrapper_t;

	void callback(wrapper_t*, const char*, int);
	
#ifdef __cplusplus
}
#endif
	

#endif /* CALLBACK_H */
