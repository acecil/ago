#ifndef AGO_H
#define AGO_H

#include <memory>

class ago
{
public:
	explicit ago(int max_conc);
	virtual ~ago();

	void go(void (*f)(void*), void *arg);
	void wait();

private:
	struct ago_impl;
	std::shared_ptr<ago_impl> impl;

	static void static_idle(ago *obj);
	void idle(void);
};

#endif	/* AGO_H */
