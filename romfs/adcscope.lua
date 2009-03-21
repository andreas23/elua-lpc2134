if pd.board() == "ET-STM32" then
  adcchannels = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}
  adcsmoothing = {4, 4, 4, 4, 16, 16, 16, 16, 32, 32, 32, 32, 64, 128, 64, 128}
  numiter = 50
else
  adcchannels = {0,1,2,3}
  adcsmoothing = {4, 16, 64, 128}
  numiter = 200
end

for i, v in ipairs(adcchannels) do
  adc.setblocking(v,1)
  adc.setclock(v,0)
  adc.setsmoothing(v,adcsmoothing[i])
end

term.clrscr()

term.gotoxy(1,1)
term.putstr("ADC Status:")
term.gotoxy(1,3)
term.putstr(" CH   SLEN   RES")
term.gotoxy(1,#adcchannels+6)
term.putstr("Press ESC to exit.")

local adcvals = {}
local ctr = 0
local key, stime, etime, dtime
local sample = adc.sample
local getsample = adc.getsample
local tread = tmr.read
local i, v

tmr.start(0)

while true do
  stime = tread(0)
  for j=1,numiter do 
    sample(adcchannels, 1)
    for i, v in ipairs(adcchannels) do
      adcvals[i] = getsample(v)
    end
  end
  etime = tread(0)
  dtime = tmr.diff(0,etime,stime)/numiter
  
  term.gotoxy(1,4)
  for i, v in ipairs(adcchannels) do
    term.putstr(string.format("ADC%d (%03d): %04d\n", v, adcsmoothing[i],adcvals[i]))
    term.gotoxy(1,i+4)
  end
  term.putstr(string.format("Tcyc: %06d (us)\n",dtime))
  
  key = term.getch( term.NOWAIT )
  if key == term.KC_ESC then break end
end

term.clrscr()
term.gotoxy( 1 , 1 )