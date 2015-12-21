function SerialCleanupFun(s)
%UNTITLED Summary of this function goes here
%   Detailed explanation goes here
disp(['INFO: ', get(s, 'Port'), ' is closed'])

fclose(s);
delete(s);
clear s

end