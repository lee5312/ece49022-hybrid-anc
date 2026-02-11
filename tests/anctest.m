clear; clc; close all;

%% ---------------- USER SETTINGS ----------------
toolWav   = "tool_noise.wav";   % tool recording
speechWav = "speech.wav";       % speech recording (should NOT be cancelled)

% Desired tool-to-speech SNR (tool louder than speech by this many dB)
desiredSNR_dB = 10;             % try 5, 10, 15 (bigger = speech harder to hear)

% Simulated digital path latency (ADC+MCU+DAC etc.)
digitalDelay_us = 200;          % try 50, 100, 200, 500

% Tool band where cancellation is focused (tune these!)
toolBandLow_Hz  = 2000;
toolBandHigh_Hz = 12000;

% Speech band used to check “don’t cancel speech”
speechBandLow_Hz  = 300;
speechBandHigh_Hz = 3400;

% Analog phase correction range (your spec)
phaseRange_deg = 45;
phaseStep_deg  = 1;

% Window used for metrics (seconds)
measureWindow_s = 0.10;         % 100 ms

% Listen?
doListen = true;

%% ---------------- LOAD AUDIO ----------------
[xTool, fs] = audioread(toolWav);
if size(xTool,2) > 1, xTool = mean(xTool,2); end
xTool = xTool / (max(abs(xTool)) + 1e-12);

[xSp, fs2] = audioread(speechWav);
if size(xSp,2) > 1, xSp = mean(xSp,2); end

% --- FIX 1: resample speech if sample rates differ ---
if fs2 ~= fs
    xSp = resample(xSp, fs, fs2);
    fprintf("Resampled speech from %d Hz to %d Hz\n", fs2, fs);
end
xSp = xSp / (max(abs(xSp)) + 1e-12);

% Match lengths (loop speech if shorter)
N = length(xTool);
if length(xSp) < N
    reps = ceil(N / length(xSp));
    xSp = repmat(xSp, reps, 1);
end
xSp = xSp(1:N);

% Limit to a manageable duration (first 8 seconds)
maxLen_s = 8;
Nmax = min(N, round(maxLen_s*fs));
xTool = xTool(1:Nmax);
xSp   = xSp(1:Nmax);

%% ---------------- CONTROLLED MIX: tool + speech ----------------
% --- FIX 2: set a controlled SNR so speech is audible and comparisons are meaningful ---
tool   = xTool / (rms(xTool) + 1e-12);
speech = xSp   / (rms(xSp)   + 1e-12);

% Make tool louder than speech by desiredSNR_dB
tool = tool * 10^(desiredSNR_dB/20);

% Mic signal (ear mic hears tool + speech)
mic = tool + speech;

t = (0:length(mic)-1)'/fs;

%% ---------------- FILTERS (band split) ----------------
bpTool = designfilt('bandpassiir','FilterOrder',4, ...
    'HalfPowerFrequency1',toolBandLow_Hz, ...
    'HalfPowerFrequency2',toolBandHigh_Hz, ...
    'SampleRate',fs);

lpTool = designfilt('lowpassiir','FilterOrder',4, ...
    'HalfPowerFrequency',toolBandLow_Hz, 'SampleRate',fs);

bpSpeech = designfilt('bandpassiir','FilterOrder',4, ...
    'HalfPowerFrequency1',speechBandLow_Hz, ...
    'HalfPowerFrequency2',speechBandHigh_Hz, ...
    'SampleRate',fs);

%% ---------------- DIGITAL ANTI-NOISE (tool-targeted, with latency) ----------------
% IMPORTANT: anti-noise is derived ONLY from the tool component (not speech)
antiIdeal = -tool;

% Simulate latency in the digital chain
delaySamp = round((digitalDelay_us*1e-6) * fs);
delaySamp = min(delaySamp, length(antiIdeal)-1); % safety
antiDigital = [zeros(delaySamp,1); antiIdeal(1:end-delaySamp)];

% Ear result with digital-only ANC
ear_digitalOnly = mic + antiDigital;

%% ---------------- ANALOG PHASE FIX (HF tool band only) ----------------
% Analog block: split antiDigital into LF + HF(tool band), shift HF only, recombine.
antiLF = filtfilt(lpTool, antiDigital);
antiHF = filtfilt(bpTool, antiDigital);

% Representative center frequency for phase correction (can be replaced by peak estimate)
f0 = sqrt(toolBandLow_Hz * toolBandHigh_Hz);

% Quick sanity print: phase error at f0 caused by digital delay
phaseErr_deg = 360 * f0 * (digitalDelay_us*1e-6);
fprintf("Delay %d us at f0=%.0f Hz -> phase error ~ %.1f deg (wraps modulo 360)\n", ...
    digitalDelay_us, f0, phaseErr_deg);

% Measurement window (avoid filter transients)
winLen = max(1, round(measureWindow_s * fs));
midIdx = floor(length(mic)/2);
winIdx = (midIdx - floor(winLen/2)) : (midIdx + ceil(winLen/2) - 1);
winIdx = max(winIdx, 1);
winIdx = min(winIdx, length(mic));

% Fractional delay helper (for phase shift simulation)
fracDelay = @(sig, dSamp) interp1((1:length(sig))', sig, (1:length(sig))' - dSamp, 'linear', 0);

% Sweep phase settings to minimize residual TOOL-band energy at the ear
phases = (-phaseRange_deg:phaseStep_deg:phaseRange_deg)';
residualToolPower = zeros(size(phases));

for k = 1:length(phases)
    phi = phases(k);

    % --- FIX 3: flip sign so correction can ADVANCE (lead) to compensate latency ---
    % Convert phase at f0 into time shift: phi = 360*f0*tau  => tau = phi/(360*f0)
    % We use tau = -phi/(360*f0) so positive phi can act like "lead" depending on convention.
    tau = -phi / (360 * f0);  % seconds
    dS  = tau * fs;           % samples (fractional allowed)

    antiHF_shift = fracDelay(antiHF, dS);
    antiAnalogFixed = antiLF + antiHF_shift;

    ear_fixed = mic + antiAnalogFixed;

    % Metric: residual power in TOOL band
    earToolBand = filtfilt(bpTool, ear_fixed);
    residualToolPower(k) = mean(earToolBand(winIdx).^2);
end

% Best phase
[~, bestIdx] = min(residualToolPower);
bestPhase = phases(bestIdx);

% Build best corrected output
tauBest = -bestPhase / (360 * f0);
dSbest  = tauBest * fs;
antiHF_best = fracDelay(antiHF, dSbest);
antiAnalogFixed_best = antiLF + antiHF_best;
ear_fixedBest = mic + antiAnalogFixed_best;

%% ---------------- METRICS: tool attenuation + speech preservation ----------------
% Tool-band RMS before/after (in window)
micToolBand = filtfilt(bpTool, mic);
digToolBand = filtfilt(bpTool, ear_digitalOnly);
fixToolBand = filtfilt(bpTool, ear_fixedBest);

rmsMicTool = rms(micToolBand(winIdx));
rmsDigTool = rms(digToolBand(winIdx));
rmsFixTool = rms(fixToolBand(winIdx));

toolAttnDig_dB = 20*log10((rmsMicTool+1e-12)/(rmsDigTool+1e-12));
toolAttnFix_dB = 20*log10((rmsMicTool+1e-12)/(rmsFixTool+1e-12));

% Speech-band impact (we WANT near 0 dB change ideally)
micSpeechBand = filtfilt(bpSpeech, mic);
digSpeechBand = filtfilt(bpSpeech, ear_digitalOnly);
fixSpeechBand = filtfilt(bpSpeech, ear_fixedBest);

rmsMicSp = rms(micSpeechBand(winIdx));
rmsDigSp = rms(digSpeechBand(winIdx));
rmsFixSp = rms(fixSpeechBand(winIdx));

speechChangeDig_dB = 20*log10((rmsDigSp+1e-12)/(rmsMicSp+1e-12));
speechChangeFix_dB = 20*log10((rmsFixSp+1e-12)/(rmsMicSp+1e-12));

%% ---------------- PLOTS ----------------
figure;
plot(phases, 10*log10(residualToolPower + 1e-18), 'LineWidth', 1.5);
grid on;
xlabel('HF phase correction (deg)');
ylabel('Residual TOOL-band power (dB)');
title(sprintf('Phase sweep (tool band %d–%d Hz, f0=%.0f Hz) | Best = %d°', ...
    toolBandLow_Hz, toolBandHigh_Hz, f0, bestPhase));

figure;
bar([toolAttnDig_dB, toolAttnFix_dB]);
grid on;
set(gca,'XTick',1,'XTickLabel',{'Tool-band attenuation'});
legend('Digital-only','Analog-assisted','Location','northwest');
ylabel('Attenuation (dB)');
title(sprintf('Tool Noise Reduction (Delay = %d \\mus, SNR = %d dB)', digitalDelay_us, desiredSNR_dB));

figure;
bar([speechChangeDig_dB, speechChangeFix_dB]);
grid on;
set(gca,'XTick',1,'XTickLabel',{'Speech-band change'});
legend('Digital-only','Analog-assisted','Location','northwest');
ylabel('Change (dB) relative to no-ANC mic');
title('Speech Preservation (target: near 0 dB, not < -5 dB)');

%% ---------------- LISTENING DEMO ----------------
if doListen
    % Normalize playback to avoid clipping
    p1 = mic             / (max(abs(mic)) + 1e-12);
    p2 = ear_digitalOnly / (max(abs(ear_digitalOnly)) + 1e-12);
    p3 = ear_fixedBest   / (max(abs(ear_fixedBest)) + 1e-12);

    fprintf("\n=== RESULTS ===\n");
    fprintf("Digital delay: %d us | Desired tool-speech SNR: %d dB\n", digitalDelay_us, desiredSNR_dB);
    fprintf("Tool-band attenuation: digital-only = %.2f dB | analog-assisted = %.2f dB\n", ...
        toolAttnDig_dB, toolAttnFix_dB);
    fprintf("Best phase setting (HF correction): %d deg at f0=%.0f Hz\n", bestPhase, f0);
    fprintf("Speech-band change:  digital-only = %.2f dB | analog-assisted = %.2f dB\n", ...
        speechChangeDig_dB, speechChangeFix_dB);
    fprintf("Note: speech-band change should be close to 0 dB (and not < -5 dB).\n");

    fprintf("\nPlaying (1) Mic: TOOL + SPEECH (no ANC)\n");
    soundsc(p1, fs); pause(min(3, length(p1)/fs) + 0.5);

    fprintf("Playing (2) Digital-only ANC (tool targeted, with delay)\n");
    soundsc(p2, fs); pause(min(3, length(p2)/fs) + 0.5);

    fprintf("Playing (3) Analog-assisted ANC (HF phase tuned)\n");
    soundsc(p3, fs); pause(min(3, length(p3)/fs) + 0.5);
end
