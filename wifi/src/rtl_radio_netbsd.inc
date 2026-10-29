/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2014 Kevin Lo <kevlo@FreeBSD.org>
 * Copyright (c) 2016 Nathanial Sloss <nathanialsloss@yahoo.com.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/* Checked PS2 transport adapter in rtl_radio.c. */

static void
urtwn_rf_write(struct urtwn_softc *sc, int chain, uint8_t addr,
    uint32_t val)
{

	urtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) | SM(R92C_LSSI_PARAM_DATA, val));
}

static uint32_t
urtwn_rf_read(struct urtwn_softc *sc, int chain, uint8_t addr)
{
	uint32_t reg[R92C_MAX_CHAINS], val;

	reg[0] = urtwn_bb_read(sc, R92C_HSSI_PARAM2(0));
	if (chain != 0) {
		reg[chain] = urtwn_bb_read(sc, R92C_HSSI_PARAM2(chain));
	}

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] & ~R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	if (urtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI) {
		val = urtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	} else {
		val = urtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	}
	return MS(val, R92C_LSSI_READBACK_DATA);
}

static void
urtwn_bb_init(struct urtwn_softc *sc)
{
	const struct urtwn_bb_prog *prog;
	uint32_t reg;
	uint8_t crystalcap;
	size_t i;

	

	

	/* Enable BB and RF. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	if (!ISSET(sc->chip, URTWN_CHIP_88E) &&
	    !ISSET(sc->chip, URTWN_CHIP_92EU)) {
		urtwn_write_1(sc, R92C_AFE_PLL_CTRL, 0x83);
		urtwn_write_1(sc, R92C_AFE_PLL_CTRL + 1, 0xdb);
	}

	urtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_USBA | R92C_SYS_FUNC_EN_USBD |
	    R92C_SYS_FUNC_EN_BB_GLB_RST | R92C_SYS_FUNC_EN_BBRSTB);

	if (!ISSET(sc->chip, URTWN_CHIP_88E) &&
	    !ISSET(sc->chip, URTWN_CHIP_92EU)) {
		urtwn_write_1(sc, R92C_LDOHCI12_CTRL, 0x0f);
		urtwn_write_1(sc, 0x15, 0xe9);
		urtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);
	}

	/* Select BB programming based on board type. */
	if (ISSET(sc->chip, URTWN_CHIP_88E))
		prog = &rtl8188eu_bb_prog;
	else if (ISSET(sc->chip, URTWN_CHIP_92EU))
		prog = &rtl8192eu_bb_prog;
	else if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD) {
			prog = &rtl8188ce_bb_prog;
		} else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA) {
			prog = &rtl8188ru_bb_prog;
		} else {
			prog = &rtl8188cu_bb_prog;
		}
	} else {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD) {
			prog = &rtl8192ce_bb_prog;
		} else {
			prog = &rtl8192cu_bb_prog;
		}
	}
	/* Write BB initialization values. */
	for (i = 0; i < prog->count; i++) {
		/* additional delay depend on registers */
		switch (prog->regs[i]) {
		case 0xfe:
			urtwn_delay_ms(sc, 50);
			break;
		case 0xfd:
			urtwn_delay_ms(sc, 5);
			break;
		case 0xfc:
			urtwn_delay_ms(sc, 1);
			break;
		case 0xfb:
			DELAY(50);
			break;
		case 0xfa:
			DELAY(5);
			break;
		case 0xf9:
			DELAY(1);
			break;
		}
		urtwn_bb_write(sc, prog->regs[i], prog->vals[i]);
		DELAY(1);
	}

	if (sc->chip & URTWN_CHIP_92C_1T2R) {
		/* 8192C 1T only configuration. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_TXINFO);
		reg = (reg & ~0x00000003) | 0x2;
		urtwn_bb_write(sc, R92C_FPGA0_TXINFO, reg);

		reg = urtwn_bb_read(sc, R92C_FPGA1_TXINFO);
		reg = (reg & ~0x00300033) | 0x00200022;
		urtwn_bb_write(sc, R92C_FPGA1_TXINFO, reg);

		reg = urtwn_bb_read(sc, R92C_CCK0_AFESETTING);
		reg = (reg & ~0xff000000) | (0x45 << 24);
		urtwn_bb_write(sc, R92C_CCK0_AFESETTING, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		reg = (reg & ~0x000000ff) | 0x23;
		urtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCPARAM1);
		reg = (reg & ~0x00000030) | (1 << 4);
		urtwn_bb_write(sc, R92C_OFDM0_AGCPARAM1, reg);

		reg = urtwn_bb_read(sc, 0xe74);
		reg = (reg & ~0x0c000000) | (2 << 26);
		urtwn_bb_write(sc, 0xe74, reg);
		reg = urtwn_bb_read(sc, 0xe78);
		reg = (reg & ~0x0c000000) | (2 << 26);
		urtwn_bb_write(sc, 0xe78, reg);
		reg = urtwn_bb_read(sc, 0xe7c);
		reg = (reg & ~0x0c000000) | (2 << 26);
		urtwn_bb_write(sc, 0xe7c, reg);
		reg = urtwn_bb_read(sc, 0xe80);
		reg = (reg & ~0x0c000000) | (2 << 26);
		urtwn_bb_write(sc, 0xe80, reg);
		reg = urtwn_bb_read(sc, 0xe88);
		reg = (reg & ~0x0c000000) | (2 << 26);
		urtwn_bb_write(sc, 0xe88, reg);
	}

	/* Write AGC values. */
	for (i = 0; i < prog->agccount; i++) {
		urtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE, prog->agcvals[i]);
		DELAY(1);
	}

	if (ISSET(sc->chip, URTWN_CHIP_88E) ||
	    ISSET(sc->chip, URTWN_CHIP_92EU)) {
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), 0x69553422);
		DELAY(1);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), 0x69553420);
		DELAY(1);

		if (ISSET(sc->chip, URTWN_CHIP_92EU)) {
			urtwn_write_2(sc, R92C_AFE_CTRL3, urtwn_read_2(sc,
			    R92C_AFE_CTRL3));
		}

		crystalcap = sc->r88e_rom[0xb9];
		if (crystalcap == 0xff)
			crystalcap = 0x20;
		crystalcap &= 0x3f;
		reg = urtwn_bb_read(sc, R92C_AFE_XTAL_CTRL);
		urtwn_bb_write(sc, R92C_AFE_XTAL_CTRL,
		    RW(reg, R92C_AFE_XTAL_CTRL_ADDR,
		    crystalcap | crystalcap << 6));
	} else {
		if (urtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
		    R92C_HSSI_PARAM2_CCK_HIPWR) {
			SET(sc->sc_flags, URTWN_FLAG_CCK_HIPWR);
		}
	}
}

static void
urtwn_rf_init(struct urtwn_softc *sc)
{
	const struct urtwn_rf_prog *prog;
	uint32_t reg, mask, saved;
	size_t i, j, idx;

	

	/* Select RF programming based on board type. */
	if (ISSET(sc->chip, URTWN_CHIP_88E))
		prog = rtl8188eu_rf_prog;
	else if (ISSET(sc->chip, URTWN_CHIP_92EU))
		prog = rtl8192eu_rf_prog;
	else if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD) {
			prog = rtl8188ce_rf_prog;
		} else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA) {
			prog = rtl8188ru_rf_prog;
		} else {
			prog = rtl8188cu_rf_prog;
		}
	} else {
		prog = rtl8192ce_rf_prog;
	}

	for (i = 0; i < sc->nrxchains; i++) {
		/* Save RF_ENV control type. */
		idx = i / 2;
		mask = 0xffffU << ((i % 2) * 16);
		saved = urtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx)) & mask;

		/* Set RF_ENV enable. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x100000;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(50);

		/* Set RF_ENV output high. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(50);

		/* Set address and data lengths of RF registers. */
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(50);
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(50);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				urtwn_delay_ms(sc, 50);
				continue;
			}
			urtwn_rf_write(sc, i, prog[i].regs[j], prog[i].vals[j]);
			DELAY(5);
		}

		/* Restore RF_ENV control type. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx)) & ~mask;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(idx), reg | saved);
	}

	if ((sc->chip & (URTWN_CHIP_UMC_A_CUT | URTWN_CHIP_92C)) ==
	    URTWN_CHIP_UMC_A_CUT) {
		urtwn_rf_write(sc, 0, R92C_RF_RX_G1, 0x30255);
		urtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x50a00);
	}

	/* Cache RF register CHNLBW. */
	for (i = 0; i < 2; i++) {
		sc->rf_chnlbw[i] = urtwn_rf_read(sc, i, R92C_RF_CHNLBW);
	}
}

static void
urtwn_pa_bias_init(struct urtwn_softc *sc)
{
	uint8_t reg;
	size_t i;

	

	

	for (i = 0; i < sc->nrxchains; i++) {
		if (sc->pa_setting & (1U << i))
			continue;

		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x0f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x4f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x8f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0xcf406);
	}
	if (!(sc->pa_setting & 0x10)) {
		reg = urtwn_read_1(sc, 0x16);
		reg = (reg & ~0xf0) | 0x90;
		urtwn_write_1(sc, 0x16, reg);
	}
}

static void
urtwn_rxfilter_init(struct urtwn_softc *sc)
{

	

	

	/* Initialize Rx filter. */
	/* TODO: use better filter for monitor mode. */
	urtwn_write_4(sc, R92C_RCR,
	    R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB |
	    R92C_RCR_APP_ICV | R92C_RCR_AMF | R92C_RCR_HTC_LOC_CTRL |
	    R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
	/* Accept all multicast frames. */
	urtwn_write_4(sc, R92C_MAR + 0, 0xffffffff);
	urtwn_write_4(sc, R92C_MAR + 4, 0xffffffff);
	/* Accept all management frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP0, 0xffff);
	/* Reject all control frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);
	/* Accept all data frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);
}

static void
urtwn_edca_init(struct urtwn_softc *sc)
{

	

	

	/* set spec SIFS (used in NAV) */
	urtwn_write_2(sc, R92C_SPEC_SIFS, 0x100a);
	urtwn_write_2(sc, R92C_MAC_SPEC_SIFS, 0x100a);

	/* set SIFS CCK/OFDM */
	urtwn_write_2(sc, R92C_SIFS_CCK, 0x100a);
	urtwn_write_2(sc, R92C_SIFS_OFDM, 0x100a);

	/* TXOP */
	urtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x005ea42b);
	urtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a44f);
	urtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005ea324);
	urtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002fa226);
}

static void
urtwn_write_txpower(struct urtwn_softc *sc, int chain,
    uint16_t power[URTWN_RIDX_COUNT])
{
	uint32_t reg;

	

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = urtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[0]);
		urtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);

		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[2]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[3]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[0]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[2]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);

		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[3]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	urtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[ 4]) |
	    SM(R92C_TXAGC_RATE09, power[ 5]) |
	    SM(R92C_TXAGC_RATE12, power[ 6]) |
	    SM(R92C_TXAGC_RATE18, power[ 7]));
	urtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[ 8]) |
	    SM(R92C_TXAGC_RATE36, power[ 9]) |
	    SM(R92C_TXAGC_RATE48, power[10]) |
	    SM(R92C_TXAGC_RATE54, power[11]));
	/* Write per-MCS Tx power. */
	urtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[12]) |
	    SM(R92C_TXAGC_MCS01,  power[13]) |
	    SM(R92C_TXAGC_MCS02,  power[14]) |
	    SM(R92C_TXAGC_MCS03,  power[15]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[16]) |
	    SM(R92C_TXAGC_MCS05,  power[17]) |
	    SM(R92C_TXAGC_MCS06,  power[18]) |
	    SM(R92C_TXAGC_MCS07,  power[19]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
	    SM(R92C_TXAGC_MCS08,  power[20]) |
	    SM(R92C_TXAGC_MCS09,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

static void
urtwn_get_txpower(struct urtwn_softc *sc, size_t chain, u_int chan, u_int ht40m,
    uint16_t power[URTWN_RIDX_COUNT])
{
	struct r92c_rom *rom = &sc->rom;
	uint16_t cckpow, ofdmpow, htpow, diff, maxpow;
	const struct urtwn_txpwr *base;
	int ridx, group;

	

	/* Determine channel group. */
	if (chan <= 3) {
		group = 0;
	} else if (chan <= 9) {
		group = 1;
	} else {
		group = 2;
	}

	/* Get original Tx power based on board type and RF chain. */
	if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_HIGHPA) {
			base = &rtl8188ru_txagc[chain];
		} else {
			base = &rtl8192cu_txagc[chain];
		}
	} else {
		base = &rtl8192cu_txagc[chain];
	}

	memset(power, 0, URTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = 0; ridx <= 3; ridx++) {
			power[ridx] = base->pwr[0][ridx];
		}
	}
	for (ridx = 4; ridx < URTWN_RIDX_COUNT; ridx++) {
		if (sc->regulatory == 3) {
			power[ridx] = base->pwr[0][ridx];
			/* Apply vendor limits. */
			if (ht40m != IEEE80211_HTINFO_2NDCHAN_NONE) {
				maxpow = rom->ht40_max_pwr[group];
			} else {
				maxpow = rom->ht20_max_pwr[group];
			}
			maxpow = (maxpow >> (chain * 4)) & 0xf;
			if (power[ridx] > maxpow) {
				power[ridx] = maxpow;
			}
		} else if (sc->regulatory == 1) {
			if (ht40m == IEEE80211_HTINFO_2NDCHAN_NONE) {
				power[ridx] = base->pwr[group][ridx];
			}
		} else if (sc->regulatory != 2) {
			power[ridx] = base->pwr[0][ridx];
		}
	}

	/* Compute per-CCK rate Tx power. */
	cckpow = rom->cck_tx_pwr[chain][group];
	for (ridx = 0; ridx <= 3; ridx++) {
		power[ridx] += cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR) {
			power[ridx] = R92C_MAX_TX_PWR;
		}
	}

	htpow = rom->ht40_1s_tx_pwr[chain][group];
	if (sc->ntxchains > 1) {
		/* Apply reduction for 2 spatial streams. */
		diff = rom->ht40_2s_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow = (htpow > diff) ? htpow - diff : 0;
	}

	/* Compute per-OFDM rate Tx power. */
	diff = rom->ofdm_tx_pwr_diff[group];
	diff = (diff >> (chain * 4)) & 0xf;
	ofdmpow = htpow + diff;	/* HT->OFDM correction. */
	for (ridx = 4; ridx <= 11; ridx++) {
		power[ridx] += ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR) {
			power[ridx] = R92C_MAX_TX_PWR;
		}
	}

	/* Compute per-MCS Tx power. */
	if (ht40m == IEEE80211_HTINFO_2NDCHAN_NONE) {
		diff = rom->ht20_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow += diff;	/* HT40->HT20 correction. */
	}
	for (ridx = 12; ridx < URTWN_RIDX_COUNT; ridx++) {
		power[ridx] += htpow;
		if (power[ridx] > R92C_MAX_TX_PWR) {
			power[ridx] = R92C_MAX_TX_PWR;
		}
	}
#ifdef URTWN_DEBUG
	if (urtwn_debug & DBG_RF) {
		/* Dump per-rate Tx power values. */
		printf("%s: %s: Tx power for chain %zd:\n",
		    device_xname(sc->sc_dev), __func__, chain);
		for (ridx = 0; ridx < URTWN_RIDX_COUNT; ridx++) {
			printf("%s: %s: Rate %d = %u\n",
			    device_xname(sc->sc_dev), __func__, ridx,
			    power[ridx]);
		}
	}
#endif
}

static void
urtwn_lc_calib(struct urtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	size_t i;

	

	

	txmode = urtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		urtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = urtwn_rf_read(sc, i, R92C_RF_AC);
			urtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}
	/* Start calibration. */
	urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    urtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	urtwn_delay_ms(sc, 100);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		urtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			urtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
		}
	} else {
		/* Unblock all Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}
