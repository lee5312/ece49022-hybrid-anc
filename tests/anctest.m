clear; close all; clc;

%% Setup
fs = 48000;
T = 2.0;
t = (0:1/fs:T-1/fs)';

% Tool tone 
f0 = 1000; % tool frequency
A = 1.0;
phi = deg2rad(45);

% Speech/ambient (uncorrelated with 1kHz references)
fspeech = 300;
Aspeech = 0.3;

% References from MCU (I and Q at f0)
I = sin(2*pi*f0*t);
Q = cos(2*pi*f0*t);

% Cal mic: correlated tool tone + uncorrelated speech
tool = A*sin(2*pi*f0*t + phi);
speech = Aspeech*sin(2*pi*fspeech*t);
d_mic = tool + speech;

%% Error WITHOUT subsystem
e_no = d_mic;

%% Error WITH subsystem 
mu = 0.001; % (Time constant of Integrator)
wI = 0; wQ = 0;
y = zeros(size(t));
e = zeros(size(t));

for n = 1:length(t)
    y(n) = wI*I(n) + wQ*Q(n); % generated anti-noise estimate
    e(n) = d_mic(n) - y(n); % residual seen by cal mic (has both residual tool noise + speech)

    % LMS update (find weights that minimize residual)
    wI = wI + mu * e(n) * I(n);
    wQ = wQ + mu * e(n) * Q(n);
end
e_yes = e;

%% Lock-in measurement at tool frequency (1kHz)
win = round(0.02*fs); % 20 ms moving-average LPF window

AI_no = movmean(e_no .* I, win);
AQ_no = movmean(e_no .* Q, win);
Acorr_tool_no = 2*sqrt(AI_no.^2 + AQ_no.^2);

AI_yes = movmean(e_yes .* I, win);
AQ_yes = movmean(e_yes .* Q, win);
Acorr_tool_yes = 2*sqrt(AI_yes.^2 + AQ_yes.^2);

%% Lock-in measurement at speech frequency (300Hz)
Is = sin(2*pi*fspeech*t);
Qs = cos(2*pi*fspeech*t);

AsI_no = movmean(e_no .* Is, win);
AsQ_no = movmean(e_no .* Qs, win);
Acorr_speech_no = 2*sqrt(AsI_no.^2 + AsQ_no.^2);

AsI_yes = movmean(e_yes .* Is, win);
AsQ_yes = movmean(e_yes .* Qs, win);
Acorr_speech_yes = 2*sqrt(AsI_yes.^2 + AsQ_yes.^2);

%% Plot 1: Correlated amplitude at tool vs speech 
figure;
plot(t, Acorr_tool_no, 'r', 'LineWidth', 1.4); hold on;
plot(t, Acorr_tool_yes,'b', 'LineWidth', 1.4);
plot(t, Acorr_speech_no,'--r','LineWidth', 1.2);
plot(t, Acorr_speech_yes,'--b','LineWidth', 1.2);
grid on;
xlabel('Time (s)');
ylabel('Estimated correlated amplitude');
title('Tool (1kHz) is minimized while speech (300Hz) is preserved');
legend('Tool @1kHz (no cancel)','Tool @1kHz (cancel)', ...
       'Speech @300Hz (no cancel)','Speech @300Hz (cancel)', ...
       'Location','best');

%% Plot 2: Raw waveforms 
figure;
subplot(3,1,1);
plot(t, d_mic); grid on;
title('Calibration mic signal: tool + speech');
ylabel('Amplitude');

subplot(3,1,2);
plot(t, y); grid on;
title('Generated cancel signal y = wI*I + wQ*Q');
ylabel('Amplitude');

subplot(3,1,3);
plot(t, e_yes); grid on;
title('Residual after cancellation (e = d\_mic - y)');
xlabel('Time (s)');
ylabel('Amplitude');

%% Plot 3: FFT overlay 
N = length(t);
w = hann(N);

E0 = fft(e_no  .* w);
E1 = fft(e_yes .* w);

f = (0:N-1)*(fs/N);
half = 1:floor(N/2);

figure;
plot(f(half), 20*log10(abs(E0(half))+1e-12), 'LineWidth', 1.2); hold on;
plot(f(half), 20*log10(abs(E1(half))+1e-12), 'LineWidth', 1.2);
grid on;
xlim([0 3000]);
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB, arbitrary)');
title('Spectrum before vs after: 1kHz reduced, speech band preserved');
legend('No cancel','With cancel','Location','best');

%% Print final learned weights
fprintf('Final weights: wI = %.4f, wQ = %.4f\n', wI, wQ);
