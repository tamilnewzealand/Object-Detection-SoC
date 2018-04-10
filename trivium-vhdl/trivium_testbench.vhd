library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity trivium_testbench is
end trivium_testbench;

architecture beh of trivium_testbench is
  signal t_clk, t_ready, t_cipher, t_decipher, t_stream_1, t_stream_2 : std_logic;
  signal t_start, t_input : std_logic := '1';
  signal t_K  : std_logic_vector(79 downto 0) := (others => '1'); 
  signal t_IV : std_logic_vector(79 downto 0) := (others => '0');  
  
  component trivium_module
    port (clk, start : in std_logic;
      K, IV : in std_logic_vector(79 downto 0);
      input : in std_logic;
      ready, output, stream_out : out std_logic);
  end component;

begin
  Trivium_Module_encipher : trivium_module port map(t_clk, t_start, t_K, t_IV, t_input, t_ready, t_cipher, t_stream_1);
  Trivium_Module_decipher : trivium_module port map(t_clk, t_start, t_K, t_IV, t_cipher, t_ready, t_decipher, t_stream_2);
    
  clk_gen: process
  begin      
    wait for 1 ms;
    t_clk <= '1';
    wait for 1 ms;
    t_clk <= '0';
  end process clk_gen;
  
  t_input <= t_clk;

end beh;