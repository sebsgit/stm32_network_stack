#include "enc28j60.h"
#include <assert.h>
#include <stdio.h>

void enc28_debug_do_reg_dump(ENC28_SPI_Context *ctx)
{
	assert(ctx);

	uint8_t reg_id;
	for (reg_id = 0x0; reg_id <= 0x1F; ++reg_id)
	{
		uint8_t reg_value = 0xFF;
		ENC28_CommandStatus status = enc28_do_read_ctl_reg(ctx, reg_id, &reg_value);
		if (status == ENC28_OK)
		{
			printf("reg read: %d = %d\n", (int)reg_id, (int)reg_value);
		}
		else
		{
			printf("error reading register %d: err code = %d\n", (int)reg_id, (int)status);
		}
	}
}

