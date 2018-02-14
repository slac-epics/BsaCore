#ifndef BSA_CORE_FACTORY_H
#define BSA_CORE_FACTORY_H

#include <BsaCore.h>
#include <BsaAlias.h>

typedef BsaAlias::shared_ptr<BsaCore> BsaCorePtr;

class BsaCoreFactory {
public:
	static const unsigned DEFAULT_BSA_LD_PATTERNBUF_SZ = 9;

private:
	unsigned ldBufSz_;
	unsigned minfill_;

public:
	BsaCoreFactory();

	BsaCoreFactory & setLdBufSz(unsigned);
	BsaCoreFactory & setMinFill(unsigned);

	BsaCorePtr create();
};

#endif
