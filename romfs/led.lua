-- eLua test 

local uartid, invert, ledpin = 0, false
if pd.board() == "SAM7-EX256" then
  ledpin = pio.PB_20
elseif pd.board() == "EK-LM3S1968" then
  ledpin = pio.PG_2
elseif pd.board() == "EK-LM3S8962" or pd.board() == "EK-LM3S6965" then
  ledpin = pio.PF_0
elseif pd.board() == "EAGLE-100" then
  ledpin = pio.PE_1
elseif pd.board() == "STR9-COMSTICK" then
  ledpin = pio.P9_0
elseif pd.board() == "LPC-H2888" then
  ledpin = pio.P2_1
elseif pd.board() == "MOD711" then
  ledpin = pio.P1_7
  uartid = 1
elseif pd.board() == "ATEVK1100" then
  ledpin = pio.PB_27
  invert = true
elseif pd.board() == "ATEVK1101" then
  ledpin = pio.PA_8
  invert = true  
  uartid = 1
elseif pd.board() == "MIZAR32" then
  ledpin = pio.PB_29
  invert = true
elseif pd.board() == "STR-E912" then
  ledpin = pio.P6_4
elseif pd.board() == "ELUA-PUC" then
  ledpin = pio.P1_20
elseif pd.board() == "MBED" then
  ledpin = mbed.pio.LED1
  mbed.pio.configpin( ledpin, 0, 0, 0 )
else
  print( "\nError: Unknown board " .. pd.board() .. " !" )
  return
end

function cycle()
  if not invert then 
    pio.pin.sethigh( ledpin ) 
  else 
    pio.pin.setlow( ledpin ) 
  end
  tmr.delay( 0, 500000 )
  if not invert then 
    pio.pin.setlow( ledpin ) 
  else 
    pio.pin.sethigh( ledpin ) 
  end
  tmr.delay( 0, 500000 )
end

pio.pin.setdir( pio.OUTPUT, ledpin )
print( "Hello from eLua on " .. pd.board() )
print "Watch your LED blinking :)"
print "Press any key to end this demo.\n"

while uart.getchar( uartid, 0 ) == "" do
  cycle()
end

