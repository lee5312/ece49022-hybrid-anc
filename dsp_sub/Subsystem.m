% Subsystem: Digital Signal Processing (H1) 
clear; close all; clc;

%% PARAMETERS
fs = 48000;          % Sampling frequency (Hz)
N  = 40000;          % Number of samples
M  = 128;             % FIR filter length (model order)
mu = 0.0001;          % Step size (learning rate)             
t  = (0:N-1)/fs;            % Time vector (matrix)
% noise = ['10khz-target_noise.wav'
%     'ballet.wav'
%     'blackbird.wav'
%     'cardbox.wav'
%     'f4.wav'];

%% 
% 1. LOAD TARGET NOISE (X)
%% 
% audioread used to extract wave signal and sampling frequency
[X, fs1] = audioread('blackbird.wav');   % Any noise file
if size(X,2) > 1
    X = mean(X,2);
end
% Trim X
X = X(1:N);
X = X / max(abs(X));

%% 
% 2. LOAD RANDOM SPEECH (Y)
%% 

[v, fs2] = audioread('speech.wav');
if size(v,2) > 1
    v = mean(v,2);
end
v = v(1:N);
v = v / max(abs(v));

%% 
% 3. CREATE RANDOM H1 (UNKNOWN PATH)
%% 
% Assume this is the real spatial transfer function (change when
% integration)
true_H1 = fir1(M-1,0.25);


%% 
% 4. CREATE MIXTURE: Y = X*H1 + v
%% 

XH1 = conv(X, true_H1);

% figure;
% subplot(3,1,1)
% plot(X, 'LineWidth', 1.5)
% title('Target Signal')
% grid on
% subplot(3,1,2)
% plot(true_H1, 'LineWidth', 1.5)
% title('Y Combined Signal')
% grid on
% subplot(3,1,3)
% plot(XH1, 'LineWidth', 1.5)
% title('Speech Signal')
% grid on


XH1 = XH1(1:N);
Y = XH1 + v;

%% 
% 5. LMS IDENTIFICATION
%% 
% Initialize g (estimated h1) and e (error) functions

mu_test_values = 0.0001:0.0001:0.01;
mu_error = zeros(1,length(mu_test_values));

% for m = 1:length(mu_test_values)
    mu = 0.005;
%     mu = mu_test_values(m);
    g = zeros(M,1);      % Estimated filter coefficients
    e = zeros(N,1);      % Error signal storage
    y_hat = zeros(N,1);
    for n = M:N
        
        x_vec = X(n:-1:n-M+1);
        
        y_hat(n) = g' * x_vec;
        
        e(n) = Y(n) - y_hat(n);
        
        g = g + mu * e(n) * x_vec;
        if (n == 10000 || (n == 20000) || (n == 30000) || (n == 40000))
            ans = 1;
        end
    end
    % Store final result
%     mu_error(m) = norm(true_H1 - g);
% end
%% 
% 6. Plot Result
%%
figure;
subplot(3,1,1)
plot(X, 'LineWidth', 1.5)
title('Target Signal')
grid on
subplot(3,1,2)
plot(Y, 'LineWidth', 1.5)
title('Y Combined Signal')
grid on
subplot(3,1,3)
plot(y_hat + v, 'LineWidth', 1.5)
title('Estimated Y')
grid on

figure;
subplot(3,1,1)
plot(true_H1, 'LineWidth', 1.5)
title('True Impulse Response')
grid on
subplot(3,1,2)
plot(g, 'LineWidth', 1.5)
title('Estimated Impulse Response (LMS)')
grid on
subplot(3,1,3)
plot(e)
title('Error Signal')
grid on

figure;
subplot(2,1,1)
plot(e-v)
title('True Error Signal')
grid on
subplot(2,1,2)
plot(abs(g'-true_H1))
title('Transfer Function Diff Signal')
grid on

%% 
% 8. Identification Error
%% 

error_norm = norm(true_H1 - g);
fprintf('Identification Error: %.4f\n', error_norm);
