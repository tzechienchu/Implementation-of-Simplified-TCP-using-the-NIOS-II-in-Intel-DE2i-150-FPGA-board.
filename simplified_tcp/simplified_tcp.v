module simplified_tcp(  // module and port declaration: fill the gaps
		
	input CLOCK_50,
	
	// KEY (reset)
	input   KEY,
	//input KEY1 , verify if needed.
	
	//LED

	output [7:0] s1_led,
	 
	//Switch
	input [7:0] s1_switch,
	
	// Ethernet : the signals used are: the RGMII transmit clock, the MDC reference, the MDIO, the hardware reset, the RGMII receive clock, the receive data, the receive data valid, the transmit data and the transmit enable (check the manual)
	output ENET_GTX_CLK,
	output ENET_MDC,
	input ENET_RX_CLK,
	inout ENET_MDIO,	
	//input ENET_TX_CLK, 
	output  [3: 0] ENET_TX_DATA,
	output   ENET_RST_N    ,
	input [3: 0] ENET_RX_DATA,
	input  ENET_RX_DV,
	output ENET_TX_EN
);

	wire sys_clk, clk_125, clk_25, clk_2p5, tx_clk;
	wire clk1_125,clk1_25,clk1_2p5;
	wire core_reset_n;
	wire mdc, mdio_in, mdio_oen, mdio_out;
	wire eth_mode, ena_10;
	//wire tx_clk1,gtk_clk1;

	// Assign MDIO and MDC signals
	
	assign mdio_in   = ENET_MDIO;
	assign ENET_MDC  = mdc;
	assign ENET_MDIO = mdio_oen ? 1'bz : mdio_out;
	
	//Assign reset
	
	assign ENET_RST_N = core_reset_n;
	
	//PLL instances
	
	my_pll pll_inst(
		.areset	(~KEY),
		.inclk0	(CLOCK_50),
		.c0		(sys_clk),
		.c1		(clk_125),
		.c2		(clk_25),
		.c3		(clk_2p5),
		.locked (core_reset_n)
	); 
	
	pll_clock_PHY1 pll_inst1(
		.areset	(~KEY),
		.inclk0	(CLOCK_50),
		.c0		(clk1_125),
		.c1		(clk1_25),
		.c2		(clk1_2p5)
	); 
	
	// Transmission Clock in FPGA (TSE IP core)
	//assign tx_clk1 = 5;
	assign tx_clk = eth_mode  ? clk_125 : ena_10 ? clk_2p5 : clk_25 ;       // GbE Mode   = 125MHz clock
	 //               ena_10?  :       // 10Mb Mode  = 2.5MHz clock
	  //                        ;         // 100Mb Mode = 25 MHz clock
	                          
	// Clock for transmission in PHY chip
	//assign gtx_clk1 = ;
	assign ENET_GTX_CLK= eth_mode ? clk1_125 : ena_10 ? clk1_2p5 : clk1_25;       // GbE Mode   = 125MHz clock
	          //      ena_10?  :       // 10Mb Mode  = 2.5MHz clock
	           //               ;         // 100Mb Mode = 25 MHz clock

	// Nios II system instance
	
    nios_system system_inst (
   .clk_clk (sys_clk),                        //  system clock (input)
   .reset_reset_n  (core_reset_n),     			//  system reset (input)
	.led_export (s1_led),								// led (output)
	.switch_export (s1_switch),							// swicht button (input)
        .tse_pcs_mac_tx_clock_connection_clk 	(tx_clk), 			//  transmit clock (input)
        .tse_pcs_mac_rx_clock_connection_clk 	(ENET_RX_CLK),		 		//  receive clock (input)
        .tse_mac_mdio_connection_mdc               (mdc),             		//  mdc (output)
        .tse_mac_mdio_connection_mdio_in         (mdio_in),           	//  mdio_in (input)
        .tse_mac_mdio_connection_mdio_out       (mdio_out),          	//  mdio_out (output)
        .tse_mac_mdio_connection_mdio_oen      (mdio_oen),     	     	//  mdio_oen (output)
        .tse_mac_rgmii_connection_rgmii_in      (ENET_RX_DATA),     //  rgmii_in (rx data, input)
        .tse_mac_rgmii_connection_rgmii_out     (ENET_TX_DATA),	   	//  gmii_out (tx data, output)
        .tse_mac_rgmii_connection_rx_control      (ENET_RX_DV),      			//  rx_control (receive data valid, input)
        .tse_mac_rgmii_connection_tx_control      (ENET_TX_EN),      			//  tx_control (tx enable, output)
        .tse_mac_status_connection_eth_mode    (eth_mode),	                         //  eth_mode (output)
        .tse_mac_status_connection_ena_10        (ena_10)          	                //   ena_10	  (output)
    );
endmodule 