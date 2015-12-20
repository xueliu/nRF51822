% instrhwinfo('serial')
clc
clear all;
close all;

port_name   = 'COM3'; % nRF51822 AK II
low_bound   = -60000;
hig_bound   = 60000;

% Check to see if there are existing serial objects (instrfind) whos 'Port' property is set to 'COM1'
oldSerial = instrfind('Port', port_name); 
if (~isempty(oldSerial)) % if the set of such objects is not(~) empty
    disp(['WARNING: ', port_name ,' in use. Closing.'])
    delete(oldSerial)
end

s = serial(port_name,'BaudRate',115200,'Parity', 'none', 'DataBits', 8 );
disp(['INFO: ', port_name ,' is opened.'])
set(s, 'Tag', 'NRF52DK');
set(s, 'TimeOut', .1);

fopen(s);
flushinput(s);
flushoutput(s);

% Clean up function when user inputs ctrl + c
finishup = onCleanup(@() SerialCleanupFun(s));

accelx      = animatedline('Color','r','LineWidth',1);
accely      = animatedline('Color','b','LineWidth',1);
accelz      = animatedline('Color','g','LineWidth',1);
accelsum    = animatedline('Color','k','LineWidth',3);

index = 1;

axis([index-40 index+5 low_bound hig_bound]);

while (true)
    out = fscanf(s);
   
    [x, y, z] = strread(out,'%f%f%f', 1, 'delimiter', ';');

    sum = (x + y + z);

    addpoints(accelx,      index, x);
    addpoints(accely,      index, y);
    addpoints(accelz,      index, z);
    addpoints(accelsum,    index, sum);

    drawnow
    axis([index-40 index+5 low_bound hig_bound]);

    index = index + 1;
end
