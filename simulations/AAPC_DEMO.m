clear; close all; clc;

%  PART A — SYNTHETIC MULTI-TONE SCENARIOS


fprintf("\nPART A: SYNTHETIC SCENARIOS\n");

%% GLOBAL SETUP
fs = 48000;
T  = 2.0;
t  = (0:1/fs:T-1/fs)';
N  = length(t);
dt = 1/fs;

%% SPEECH / AMBIENT (UNCORRELATED)
fspeech = 300;
As0 = 0.25; Asvar = 0.6; fAs = 1.2;
speech_env = As0 * (1 + Asvar*sin(2*pi*fAs*t));
speech = speech_env .* sin(2*pi*fspeech*t);

% Add bursts to mimic nonstationary ambient
rng(1);
burst = zeros(N,1);
numBursts = 5;
burstLen  = round(0.06*fs);
for k = 1:numBursts
    idx0 = randi([1, N-burstLen]);
    burst(idx0:idx0+burstLen) = burst(idx0:idx0+burstLen) + 0.5*hann(burstLen+1);
end
speech = speech + burst .* sin(2*pi*(fspeech+80)*t);

%% POWER AMP + PROP DELAY (SAME FOR ALL)
Gamp = 5;
Vmax = 2.0;
delay_s = 0.0002;
Nd = max(1, round(delay_s*fs));

mu = 48;

%% MCU "ALMOST PERFECT ANTI-NOISE" MISMATCH
Gmcu = 0.95;
tau_mcu = 80e-6;
Nmcu = max(0, round(tau_mcu*fs));

%% SCENARIOS (3 TOTAL)
scenariosA = {
    struct('name','SINGLE 1.0 kHz (NO DRIFT)', ...
           'tones',[1000], ...
           'amps',[1.0], ...
           'driftHz',[0]), ...

    struct('name','CLUSTERED 1.0–1.3 kHz (MILD DRIFT)', ...
           'tones',[1000 1150 1300], ...
           'amps',[1.0 0.7 0.5], ...
           'driftHz',[10 10 10]), ...

    struct('name','WIDE 0.8–6 kHz (MANY STRONGISH TONES)', ...
           'tones',[800 1200 2500 4000 6000], ...
           'amps',[1.0 0.7 0.5 0.35 0.25], ...
           'driftHz',[10 15 20 25 30])
};

%% RUN ALL SYNTHETIC SCENARIOS
for s = 1:numel(scenariosA)

    cfg = scenariosA{s};
    rng(100 + s);

    % Build TOOL
    tool = zeros(N,1);

    for k = 1:numel(cfg.tones)
        f0 = cfg.tones(k);
        A0 = cfg.amps(k);

        df = cfg.driftHz(k);
        if df == 0
            theta = 2*pi*f0*t;
        else
            fd = 0.5 + 0.15*k;
            f_inst = f0 + df*sin(2*pi*fd*t);
            theta = 2*pi*cumsum(f_inst)*dt;
        end

        phi0 = 2*pi*rand;
        tool = tool + A0*sin(theta + phi0);
    end

    % Calibration mic (no-cancel)
    d_mic = tool + speech + 0.01*randn(N,1);
    e_no = d_mic;

    % MCU antinoise mismatch
    anti_mcu = Gmcu * [zeros(Nmcu,1); tool(1:end-Nmcu)];

    % Input
    I = anti_mcu;
    Q = imag(hilbert(I));

    % Adaptive loop
    wI = 0; wQ = 0;
    y  = zeros(N,1);
    e  = zeros(N,1);
    ybuf = zeros(Nd,1);

    for n = 1:N
        y_pre = wI*I(n) + wQ*Q(n);

        y_lin = Gamp * y_pre;
        y_amp = max(min(y_lin, Vmax), -Vmax);

        y_del = ybuf(1);
        ybuf(1:end-1) = ybuf(2:end);
        ybuf(end) = y_amp;

        e(n) = d_mic(n) - y_del;

        wI = wI + mu * e(n) * I(n) * dt;
        wQ = wQ + mu * e(n) * Q(n) * dt;

        y(n) = y_del;
    end

    e_yes = e;

    % Ground-truth tool component at mic (tool + noise) by subtracting speech
    e_tool_no  = e_no  - speech;
    e_tool_yes = e_yes - speech;

    % RMS envelopes (tool only)
    win = round(0.02*fs);
    tool_rms_no  = sqrt(movmean(e_tool_no.^2,  win));
    tool_rms_yes = sqrt(movmean(e_tool_yes.^2, win));

    % === SINGLE RMS FIGURE (TOOL ONLY) ===
    figure('Name',['PART A | ' cfg.name ' | Tool RMS Before vs After']);
    plot(t, tool_rms_no,  'LineWidth', 1.2); hold on;
    plot(t, tool_rms_yes, 'LineWidth', 1.2);
    grid on;
    xlabel('Time (s)'); ylabel('RMS (arb)');
    title(['PART A | ' cfg.name ' | Tool RMS (no cancel) vs (cancel)']);
    legend('Tool RMS (no cancel)','Tool RMS (cancel)','Location','best');

    % === TIME DOMAIN FIGURE (UNCHANGED) ===
    figure('Name',['PART A | ' cfg.name ' | Time Domain']);

    subplot(4,1,1);
    plot(t, d_mic); grid on;
    title(['PART A | ' cfg.name ' | Calibration mic = tool + speech']);
    ylabel('Amp');

    subplot(4,1,2);
    plot(t, speech, 'LineWidth', 1.2); grid on;
    title('Speech / ambient only (ground truth — should NOT be cancelled)');
    ylabel('Amp');

    subplot(4,1,3);
    plot(t, e_yes); grid on;
    title('Residual after cancellation');
    ylabel('Amp');

    subplot(4,1,4);
    plot(t, e_yes - speech, 'LineWidth', 1.2); grid on;
    title('Residual minus speech (ideally \approx 0)');
    xlabel('Time (s)');
    ylabel('Amp');

    % Console summary
    eps0 = 1e-12;
    overall_no  = rms(e_tool_no)  + eps0;
    overall_yes = rms(e_tool_yes) + eps0;
    overall_attn = 20*log10(overall_yes/overall_no);

    fprintf('\n=== PART A | %s ===\n', cfg.name);
    fprintf('Final weights: wI = %.4f, wQ = %.4f\n', wI, wQ);
    fprintf('Overall tool attenuation (RMS): %.2f dB\n', overall_attn);
end


%  PART B — REAL AUDIO FILE SCENARIOS (PERFECT ONLY)


fprintf("\nPART B: REAL AUDIO SCENARIOS (PERFECT ONLY)\n");

%% AUDIO FILES
tool_file   = "Electric_Drill.m4a";
speech_file = "Speech_Ambient.m4a";

%% GLOBAL SETUP
fs_target = 48000;
T = 2.0;
eps0 = 1e-12;

%% LOAD AUDIO
assert(isfile(tool_file),   "Missing file: %s", tool_file);
assert(isfile(speech_file), "Missing file: %s", speech_file);

[x_tool,  fs_tool]  = audioread(tool_file);
[x_sp,    fs_sp]    = audioread(speech_file);

% mono
if size(x_tool,2) > 1, x_tool = mean(x_tool,2); end
if size(x_sp,2)   > 1, x_sp   = mean(x_sp,2);   end

% resample
if fs_tool ~= fs_target, x_tool = resample(x_tool, fs_target, fs_tool); end
if fs_sp   ~= fs_target, x_sp   = resample(x_sp,   fs_target, fs_sp);   end

fs = fs_target;

% trim
Nmax = round(T*fs);
N = min([length(x_tool), length(x_sp), Nmax]);

tool_raw   = x_tool(1:N);
speech_raw = x_sp(1:N);

t  = (0:N-1)'/fs;
dt = 1/fs;

%% NORMALIZE + SCALE (REALISTIC MIX)
tool_rms0   = rms(tool_raw)   + eps0;
speech_rms0 = rms(speech_raw) + eps0;

tool   = tool_raw   / tool_rms0;
speech = speech_raw / speech_rms0;

tool_level   = 1.0;
speech_level = 0.35;

tool   = tool_level   * tool;
speech = speech_level * speech;

%% MIC NOISE + CAL MIC
rng(1);
meas_noise = 0.01 * randn(N,1);
d_mic = tool + speech + meas_noise;
e_no = d_mic;

%% POWER AMP + PROP DELAY
Gamp = 5;
Vmax = 2.0;
delay_s = 0.0002;
Nd = max(1, round(delay_s * fs));

mu = 48;

%% "HOW TOOL LOOKS LIKE" FIGURE (KEEP)
figure('Name','PART B | Tool Signal: Time + FFT + PSD');

subplot(3,1,1);
plot(t, tool); grid on;
title('PART B | Tool waveform (time domain)');
xlabel('Time (s)'); ylabel('Amplitude');

Nfft = 2^nextpow2(N);
w = hann(N);
ToolFFT = fft(tool .* w, Nfft);
f = (0:Nfft-1)*(fs/Nfft);
half = 1:floor(Nfft/2);
magdB = 20*log10(abs(ToolFFT(half))+eps0);

subplot(3,1,2);
plot(f(half), magdB, 'LineWidth', 1.1);
grid on; xlim([0 8000]);
title('PART B | Tool FFT magnitude (0–8 kHz)');
xlabel('Frequency (Hz)'); ylabel('Magnitude (dB)');

subplot(3,1,3);
[pxx,f_psd] = pwelch(tool, hann(4096), 2048, 4096, fs);
plot(f_psd, 10*log10(pxx+eps0), 'LineWidth', 1.1); grid on;
xlim([0 8000]);
title('PART B | Tool Power Spectral Density (Welch)');
xlabel('Frequency (Hz)'); ylabel('PSD (dB/Hz)');

%% RUN ONLY PERFECT ANTINOISE SCENARIO
tool_adv = [tool(1+Nd:end); zeros(Nd,1)];
anti_mcu = (1/Gamp) * tool_adv;

Iref = anti_mcu;
Qref = imag(hilbert(Iref));

wI = 1.0; wQ = 0.0;
freeze_if_small = true;

y  = zeros(N,1);
e  = zeros(N,1);
ybuf = zeros(Nd,1);

for n = 1:N
    y_pre = wI*Iref(n) + wQ*Qref(n);

    y_lin = Gamp * y_pre;
    y_amp = max(min(y_lin, Vmax), -Vmax);

    y_del = ybuf(1);
    ybuf(1:end-1) = ybuf(2:end);
    ybuf(end) = y_amp;

    e(n) = d_mic(n) - y_del;

    if freeze_if_small
        if abs(e(n)) > 1e-3
            wI = wI + mu * e(n) * Iref(n) * dt;
            wQ = wQ + mu * e(n) * Qref(n) * dt;
        end
    else
        wI = wI + mu * e(n) * Iref(n) * dt;
        wQ = wQ + mu * e(n) * Qref(n) * dt;
    end

    y(n) = y_del;
end

e_yes = e;

%% PART B TOOL RMS FIGURE (TOOL ONLY)
e_tool_no  = e_no  - speech;
e_tool_yes = e_yes - speech;

win = round(0.02*fs);
tool_rms_no  = sqrt(movmean(e_tool_no.^2,  win));
tool_rms_yes = sqrt(movmean(e_tool_yes.^2, win));

figure('Name','PART B | Tool RMS Before vs After | PERFECT ONLY');
plot(t, tool_rms_no,  'LineWidth', 1.2); hold on;
plot(t, tool_rms_yes, 'LineWidth', 1.2);
grid on;
xlabel('Time (s)'); ylabel('RMS (arb)');
title('PART B | Tool RMS (no cancel) vs (cancel) — Perfect antinoise');
legend('Tool RMS (no cancel)','Tool RMS (cancel)','Location','best');

%% PART B TIME DOMAIN FIGURE (KEEP)
figure('Name','PART B | Time Domain | PERFECT ONLY');

subplot(4,1,1);
plot(t, d_mic); grid on;
title('PART B | Calibration mic: tool + speech + noise | PERFECT ONLY');
ylabel('Amp');

subplot(4,1,2);
plot(t, speech, 'LineWidth', 1.1); grid on;
title('Speech/Ambient only (ground truth — should NOT be cancelled)');
ylabel('Amp');

subplot(4,1,3);
plot(t, e_yes); grid on;
title('Residual after cancellation');
ylabel('Amp');

subplot(4,1,4);
plot(t, e_yes - speech, 'LineWidth', 1.1); grid on;
title('Residual minus speech (ideally \approx 0)');
xlabel('Time (s)');
ylabel('Amp');

%% Console summary
overall_no  = rms(e_tool_no)  + eps0;
overall_yes = rms(e_tool_yes) + eps0;
overall_attn_dB = 20*log10(overall_yes/overall_no);

fprintf("\n=== PART B | PERFECT ONLY ===\n");
fprintf("Final weights: wI = %.4f, wQ = %.4f\n", wI, wQ);
fprintf("Overall tool attenuation (RMS): %.2f dB\n", overall_attn_dB);

fprintf("\nDONE: PLOTS GENERATED\n");
