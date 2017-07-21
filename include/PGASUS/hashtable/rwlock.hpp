/*

Ideas for RW-lock

FLAG_WRITE      ...

uint32_t atomic;

read:
	while true:
		v = atomic;
		if (v & FLAG_WRITE)
			backoff()
		else if (xchg(atomic, v+1)
			break

done_read:
	atomic_sub(v, 1);

write:
	// set "WRITE" flag
	while true:
		v = atomic
		if (v & FLAG_WRITE)
			backoff()
		else if (xchg(atomic, v | FLAG_WRITE))
			break;
	
	// wait till readers are done
	while atomic.load != 0:
		backoff()
