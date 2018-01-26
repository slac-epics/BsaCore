#include <RingBufferSync.h>
#include <stdexcept>
#include <stdio.h>

typedef RingBufferSync<int> RBS;

#define LDS 2

int
main()
{
RBS dut(LDS,(1<<(LDS-1)));
int i;
int cnt=0;

	for ( i=0; i<(1<<LDS); i++ ) {
		if ( ! dut.push_back(cnt, false) ) {
			throw std::runtime_error("Test FAILED (unable to push)");
		}
		cnt++;
	}
	if ( dut.push_back(cnt, false) ) {
		throw std::runtime_error("Test FAILED (push past size succeeded)");
	}
	if ( dut.size() != (1<<LDS) ) {
		throw std::runtime_error("Test FAILED (incorrect size)");
	}
	for ( i=0; i<(1<<(LDS-1)); i++ ) {
		if ( ! dut.tryWait() ) {
			throw std::runtime_error("Test FAILED (nothing to read)");
		}
		if ( dut.front() != i ) {
			throw std::runtime_error("Test FAILED (incorrect readback)");
		}
		dut.pop();
	}
	if ( dut.tryWait() ) {
		throw std::runtime_error("Test FAILED (pop past half-fill succeeded)");
	}
	if ( ! dut.push_back( cnt ) ) {
		throw std::runtime_error("Test FAILED (push after read failed)");
	}
	cnt++;
	if ( ! dut.tryWait() ) {
		throw std::runtime_error("Test FAILED (nothing to read after 2nd push)");
	}
	if ( dut.front() != i ) {
		throw std::runtime_error("Test FAILED (incorrect readback (after 2nd))");
	}
	dut.pop();
	if ( dut.tryWait() ) {
		throw std::runtime_error("Test FAILED (pop past half-fill succeeded (2nd time))");
	}
	printf("TEST PASSED\n");
	return 0;
}
