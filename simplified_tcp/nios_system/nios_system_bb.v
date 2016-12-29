
module nios_system (
	clk_clk,
	led_export,
	reset_reset_n,
	switch_export,
	tse_mac_mdio_connection_mdc,
	tse_mac_mdio_connection_mdio_in,
	tse_mac_mdio_connection_mdio_out,
	tse_mac_mdio_connection_mdio_oen,
	tse_mac_rgmii_connection_rgmii_in,
	tse_mac_rgmii_connection_rgmii_out,
	tse_mac_rgmii_connection_rx_control,
	tse_mac_rgmii_connection_tx_control,
	tse_mac_status_connection_set_10,
	tse_mac_status_connection_set_1000,
	tse_mac_status_connection_eth_mode,
	tse_mac_status_connection_ena_10,
	tse_pcs_mac_rx_clock_connection_clk,
	tse_pcs_mac_tx_clock_connection_clk);	

	input		clk_clk;
	output	[7:0]	led_export;
	input		reset_reset_n;
	input	[7:0]	switch_export;
	output		tse_mac_mdio_connection_mdc;
	input		tse_mac_mdio_connection_mdio_in;
	output		tse_mac_mdio_connection_mdio_out;
	output		tse_mac_mdio_connection_mdio_oen;
	input	[3:0]	tse_mac_rgmii_connection_rgmii_in;
	output	[3:0]	tse_mac_rgmii_connection_rgmii_out;
	input		tse_mac_rgmii_connection_rx_control;
	output		tse_mac_rgmii_connection_tx_control;
	input		tse_mac_status_connection_set_10;
	input		tse_mac_status_connection_set_1000;
	output		tse_mac_status_connection_eth_mode;
	output		tse_mac_status_connection_ena_10;
	input		tse_pcs_mac_rx_clock_connection_clk;
	input		tse_pcs_mac_tx_clock_connection_clk;
endmodule
