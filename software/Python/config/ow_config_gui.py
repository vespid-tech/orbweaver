from pathlib import Path
from contextlib import redirect_stderr, redirect_stdout
import random
import secrets
import sys
import threading
import tkinter as tk
from ctypes import sizeof
from tkinter import filedialog, messagebox, ttk

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import serial
import serial.tools.list_ports

from config.load_via_app_usb import load_via_serial as load_via_app_serial
from config.load_via_bl_usb import load_via_bl_serial
from config.ow_config import (
    bitrate_set,
    cipher_key_set,
    configuration_get,
    encryption_set,
    freq_band_set,
    radio_addr_set,
    radio_reset,
)

BITRATE_VALUES = {
    "625 bps (not recommended)": 1,
    "2.5 kbps": 2,
    "5 kbps": 3,
    "50 kbps": 4,
    "200 kbps": 5,
    "300 kbps": 6,
    "400 kbps": 7,
    "500 kbps": 8,
}

CONFIGURATION_BAUDRATE = 460800
CHANNEL_COUNT_ITU_2 = 53


class Configurator:
    def __init__(self, root):
        self.root = root
        self.root.title("Orbweaver Configurator")
        self.root.geometry("650x900")
        self.root.minsize(600, 350)

        self.file_path = tk.StringVar()
        self.com_port = tk.StringVar()
        self.load_mode = tk.StringVar(value="Bootloader")
        self.radio_address = tk.IntVar(value=255)
        self.load_address = tk.IntVar(value=254)
        self.itu_region = tk.StringVar(value="ITU region 2 (902-928 MHz)")
        self.frequency_min = 902
        self.cipher_key = tk.StringVar()
        self.encryption_enabled = False
        self.address = tk.StringVar()
        self.channel_list = tk.StringVar()
        self.default_channel = tk.IntVar(value=26)
        self.frequency = tk.StringVar()
        self.bitrate = tk.StringVar(value="50 kbps")
        self.channels = []
        self._control_states = {}

        self.default_channel.trace_add("write", self.update_frequency)
        self.update_frequency()
        self._build_ui()
        self.refresh_ports()

    def _build_ui(self):
        ttk.Label(self.root, text="Source Address (1-254):").grid(
            row=0, column=0, padx=(12, 8), pady=(12, 12), sticky="w"
        )
        ttk.Spinbox(
            self.root, from_=1, to=254, textvariable=self.load_address, width=14
        ).grid(row=0, column=0, padx=(170, 12), pady=(12, 12), sticky="w")

        self.radio_address_label = ttk.Label(self.root, text="Radio Address (1-255):")
        self.radio_address_label.grid(
            row=1, column=0, padx=(12, 8), pady=(0, 12), sticky="w"
        )
        self.radio_address_selector = ttk.Spinbox(
            self.root, from_=1, to=255, textvariable=self.radio_address, width=14
        )
        self.radio_address_selector.grid(
            row=1, column=0, padx=(170, 12), pady=(0, 12), sticky="w"
        )

        ttk.Label(self.root, text="COM port:").grid(
            row=2, column=0, padx=(12, 8), pady=(0, 12), sticky="w"
        )
        self.port_selector = ttk.Combobox(
            self.root, textvariable=self.com_port, state="readonly", width=18
        )
        self.port_selector.grid(row=2, column=0, padx=(170, 12), pady=(0, 12), sticky="w")
        ttk.Button(self.root, text="Refresh", command=self.refresh_ports).grid(
            row=2, column=0, padx=(350, 12), pady=(0, 12), sticky="w"
        )

        settings = ttk.LabelFrame(self.root, text="Load", padding=12)
        settings.grid(row=3, column=0, padx=12, pady=(0, 12), sticky="ew")

        ttk.Label(settings, text="Load mode:").grid(row=0, column=0, padx=(0, 8), pady=6, sticky="w")
        self.load_mode_selector = ttk.Combobox(
            settings,
            textvariable=self.load_mode,
            values=("Bootloader", "Application"),
            state="readonly",
            width=18,
        )
        self.load_mode_selector.grid(row=0, column=1, padx=(0, 8), pady=6, sticky="w")
        self.load_mode_selector.bind("<<ComboboxSelected>>", self.load_mode_changed)

        ttk.Entry(settings, textvariable=self.file_path, width=48).grid(
            row=1, column=1, padx=(0, 8), pady=6
        )
        ttk.Label(settings, text="Binary File Path:").grid(row=1, column=0, padx=(0, 8), pady=6, sticky="w")
        ttk.Button(settings, text="Browse...", command=self.select_file).grid(row=1, column=2, pady=6)

        self.load_button = ttk.Button(
            settings, text="Load Binary", command=self.load, width=18
        )
        self.load_button.grid(row=2, column=0, pady=(12, 0), sticky="w")

        configuration = ttk.LabelFrame(self.root, text="Configuration", padding=12)
        configuration.grid(row=4, column=0, padx=12, pady=(0, 12), sticky="ew")

        validate_radio_address = (
            self.root.register(self.validate_address), "%P", "255"
        )
        ttk.Label(configuration, text="Address (1-254):").grid(
            row=0, column=0, padx=(0, 8), pady=5, sticky="w"
        )
        ttk.Entry(
            configuration,
            textvariable=self.address,
            validate="key",
            validatecommand=validate_radio_address,
            width=14,
        ).grid(
            row=0, column=1, padx=(0, 16), pady=5, sticky="w"
        )
        ttk.Button(configuration, text="Set New Address", command=self.set_new_address, width=18).grid(
            row=0, column=4, padx=(0, 8), pady=5
        )
        ttk.Label(configuration, text="Bitrate:").grid(
            row=1, column=0, padx=(0, 8), pady=5, sticky="w"
        )
        ttk.Combobox(
            configuration,
            textvariable=self.bitrate,
            values=(
                "625 bps",
                "2.5 kbps",
                "5 kbps",
                "50 kbps",
                "200 kbps",
                "300 kbps",
                "400 kbps",
                "500 kbps",
            ),
            state="readonly",
            width=14,
        ).grid(row=1, column=1, padx=(0, 16), pady=5, sticky="w")
        ttk.Button(configuration, text="Set Bitrate", command=self.set_bitrate, width=18).grid(
            row=1, column=4, padx=(0, 8), pady=5
        )

        ttk.Label(configuration, text="Cipher Key:").grid(
            row=3, column=0, padx=(0, 8), pady=5, sticky="w"
        )
        ttk.Entry(configuration, textvariable=self.cipher_key, width=34).grid(
            row=3, column=1, padx=(0, 8), pady=5, sticky="w"
        )
        ttk.Button(
            configuration, text="Generate Key", command=self.generate_cipher_key, width=18
        ).grid(row=3, column=2, padx=(0, 8), pady=5)
        ttk.Button(configuration, text="Set Cipher Key", command=self.set_cipher_key, width=18).grid(
            row=3, column=4, padx=(0, 8), pady=5
        )
        self.encryption_button = ttk.Button(
            configuration, text="Encryption Disabled", command=self.set_encryption
        )
        self.encryption_button.grid(row=4, column=1, padx=(0, 8), pady=5, sticky="w")

        ttk.Button(configuration, text="Set Frequency Band", command=self.set_frequency_band, width=18).grid(
            row=7, column=4, padx=(0, 8), pady=5
        )
        ttk.Label(configuration, text="Frequency Band:").grid(row=5, column=0, padx=(0, 8), pady=5, sticky="w")
        region_selector = ttk.Combobox(
            configuration,
            textvariable=self.itu_region,
            values=("ITU Region 1 (863-870 MHz)", "ITU region 2 (902-928 MHz)"),
            state="readonly",
            width=25,
        )
        region_selector.grid(row=5, column=1, padx=(0, 16), pady=5, sticky="w")
        region_selector.bind("<<ComboboxSelected>>", self.region_changed)

        ttk.Label(configuration, text="Available Channels:").grid(
            row=6, column=0, padx=(0, 8), pady=5, sticky="w"
        )
        self.channel_display = tk.Text(
            configuration, height=6, width=30, state="disabled", wrap="word"
        )
        self.channel_display.grid(
            row=6, column=1, columnspan=3, padx=(0, 8), pady=5, sticky="w"
        )

        ttk.Label(configuration, text="Default Channel:").grid(
            row=7, column=0, padx=(0, 8), pady=5, sticky="w"
        )
        validate_default_channel = (self.root.register(self.validate_default_channel), "%P")
        default_channel_frame = ttk.Frame(configuration)
        default_channel_frame.grid(row=7, column=1, columnspan=2, padx=0, pady=5, sticky="w")
        self.default_channel_selector = ttk.Spinbox(
            default_channel_frame,
            from_=0,
            to=52,
            textvariable=self.default_channel,
            validate="key",
            validatecommand=validate_default_channel,
            width=14,
        )
        self.default_channel_selector.pack(side="left")
        ttk.Label(default_channel_frame, textvariable=self.frequency).pack(
            side="left", padx=(4, 0)
        )
        
        self.get_configuration_button = ttk.Button(
            configuration, text="Get Configuration", command=self.get_configuration, width=18
        )
        self.get_configuration_button.grid(
            row=9, column=0, padx=(0, 8), pady=(12, 0), sticky="w"
        )
        ttk.Button(
            configuration, text="Reset Radio", command=self.reset_radio
        ).grid(row=9, column=1, padx=(0, 8), pady=(12, 0), sticky="w")

        self.region_changed()

        output_frame = ttk.LabelFrame(self.root, text="Output", padding=8)
        output_frame.grid(row=5, column=0, padx=12, pady=(0, 12), sticky="nsew")
        self.root.rowconfigure(5, weight=1)
        self.root.columnconfigure(0, weight=1)
        output_frame.rowconfigure(0, weight=1)
        output_frame.columnconfigure(0, weight=1)

        self.output = tk.Text(output_frame, height=14, state="disabled", wrap="none")
        self.output.grid(row=0, column=0, sticky="nsew")
        scrollbar = ttk.Scrollbar(output_frame, command=self.output.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.output.configure(yscrollcommand=scrollbar.set)

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_selector["values"] = ports
        if ports:
            self.port_selector.current(0)
        else:
            self.com_port.set("")

    def load_mode_changed(self, event=None):
        self.radio_address_selector.configure(state="normal")
        self.radio_address_label.configure(state="normal")

    def select_file(self):
        selected_file = filedialog.askopenfilename(
            title="Select firmware file",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
        )
        if selected_file:
            self.file_path.set(selected_file)

    def region_changed(self, event=None):
        if self.itu_region.get() == "ITU Region 1 (863-870 MHz)":
            maximum_channel = 14
            self.frequency_min = 863
            self.default_channel.set(7)
        else:
            maximum_channel = 52
            self.frequency_min = 902
            self.default_channel.set(26)
        self.default_channel_selector.configure(to=maximum_channel)
        self.channels = list(range(maximum_channel + 1))
        self.update_channel_display()

    def randomize_channels(self):
        random.shuffle(self.channels)
        self.update_channel_display()

    def update_channel_display(self):
        channel_text = " ".join(str(channel) for channel in self.channels)
        self.channel_list.set(channel_text)
        self.channel_display.configure(state="normal")
        self.channel_display.delete("1.0", tk.END)
        self.channel_display.insert("1.0", channel_text)
        self.channel_display.configure(state="disabled")

    def update_frequency(self, *_):
        try:
            frequency = self.frequency_min + (self.default_channel.get() / 2)
        except tk.TclError:
            return
        self.frequency.set(f"{frequency:.1f} MHz")

    def validate_default_channel(self, proposed_value):
        if not proposed_value:
            return True
        if not proposed_value.isdigit():
            return False
        return int(proposed_value) <= int(self.default_channel_selector.cget("to"))

    def validate_address(self, proposed_value, maximum):
        if not proposed_value:
            return True
        return proposed_value.isdigit() and 1 <= int(proposed_value) <= int(maximum)

    def generate_cipher_key(self):
        self.cipher_key.set(secrets.token_hex(16))

    def _set_controls_busy(self, busy):
        control_types = (
            ttk.Button,
            ttk.Combobox,
            ttk.Entry,
            ttk.Spinbox,
            tk.Entry,
            tk.Spinbox,
        )

        def visit(widget):
            if isinstance(widget, control_types):
                if busy:
                    self._control_states[widget] = widget.cget("state")
                    widget.config(state="disabled")
                else:
                    widget.config(state=self._control_states.pop(widget, "normal"))
            for child in widget.winfo_children():
                visit(child)

        visit(self.root)

    def _start_operation(self, target, args):
        self._set_controls_busy(True)
        threading.Thread(target=target, args=args, daemon=True).start()

    def _run_with_gui_output(self, target, *args):
        gui_output = _GuiOutput(self.root, self.write_output)
        try:
            with redirect_stdout(gui_output), redirect_stderr(gui_output):
                target(*args)
        finally:
            self.root.after(0, self._set_controls_busy, False)

    def set_new_address(self):
        try:
            source_address = self.load_address.get()
            radio_address = self.radio_address.get()
            new_address = int(self.address.get())
        except (tk.TclError, ValueError):
            messagebox.showerror("Address error", "Enter a valid new address.")
            return

        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Address error", "Select a COM port first.")
            return

        self._start_operation(
            self._run_with_gui_output,
            (radio_addr_set, com_port, source_address, radio_address, new_address),
        )

    def set_bitrate(self):
        try:
            source_address = self.load_address.get()
            radio_address = self.radio_address.get()
            bitrate = BITRATE_VALUES[self.bitrate.get()]
        except (KeyError, tk.TclError, ValueError):
            messagebox.showerror("Bitrate error", "Select a valid bitrate.")
            return

        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Bitrate error", "Select a COM port first.")
            return

        self._start_operation(
            self._run_with_gui_output,
            (bitrate_set, com_port, source_address, radio_address, bitrate),
        )

    def set_frequency_band(self):
        frequency_band = {
            "ITU Region 1 (863-870 MHz)": 1,
            "ITU region 2 (902-928 MHz)": 2,
        }.get(self.itu_region.get())
        if frequency_band is None:
            messagebox.showerror("Frequency band error", "Select a valid frequency band.")
            return

        source_address = self.load_address.get()
        radio_address = self.radio_address.get()
        default_channel = self.default_channel.get()
        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Frequency band error", "Select a COM port first.")
            return

        self._start_operation(
            self._run_with_gui_output,
            (freq_band_set, com_port, source_address, radio_address, frequency_band, default_channel),
        )

    def set_cipher_key(self):
        cipher_key = self.cipher_key.get()
        if len(cipher_key) != 32 or any(
            character not in "0123456789abcdefABCDEF" for character in cipher_key
        ):
            messagebox.showerror(
                "Cipher key error",
                "Cipher key must contain exactly 32 hexadecimal characters.",
            )
            return

        source_address = self.load_address.get()
        radio_address = self.radio_address.get()
        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Cipher key error", "Select a COM port first.")
            return

        self._start_operation(
            self._run_with_gui_output,
            (cipher_key_set, com_port, source_address, radio_address, cipher_key),
        )

    def set_encryption(self):
        source_address = self.load_address.get()
        radio_address = self.radio_address.get()
        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Encryption error", "Select a COM port first.")
            return

        self.encryption_enabled = not self.encryption_enabled
        state = "Enabled" if self.encryption_enabled else "Disabled"
        self.encryption_button.config(text=f"Encryption {state}")
        self._start_operation(
            self._run_with_gui_output,
            (encryption_set, com_port, source_address, radio_address, self.encryption_enabled),
        )

    def load_configuration(self):
        key = self.cipher_key.get()
        if len(key) != 32 or any(character not in "0123456789abcdefABCDEF" for character in key):
            messagebox.showerror(
                "Configuration error", "Cipher key must contain exactly 32 hexadecimal characters."
            )
            return

        try:
            device_address = int(self.address.get())
        except ValueError:
            device_address = -1
        if not 0 <= device_address <= 255:
            messagebox.showerror("Configuration error", "Address must be a number from 0 to 255.")
            return

        try:
            packet = self.build_configuration_packet(device_address, key)
        except (KeyError, ValueError, ModuleNotFoundError) as error:
            messagebox.showerror("Configuration error", str(error))
            return

        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Configuration error", "Select a COM port first.")
            return

        self.write_output(
            f"Sending configuration to {com_port} at {CONFIGURATION_BAUDRATE} baud...\n"
        )
        self._start_operation(self._run_configuration, (com_port, packet))

    def build_configuration_packet(self, device_address, key):
        from cmd_proc import START_BYTE, STOP_BYTE, system_cmd_header_t, system_cmd_t

        bitrate = BITRATE_VALUES[self.bitrate.get()]
        region = 1 if self.itu_region.get() == "ITU Region 1 (863-870 MHz)" else 2
        key_bytes = bytes.fromhex(key)
        if len(key_bytes) != 16:
            raise ValueError("Cipher key must contain exactly 16 bytes.")

        key_words = [
            int.from_bytes(key_bytes[index:index + 4], byteorder="little")
            for index in range(0, 16, 4)
        ]
        channels = self.channels[:CHANNEL_COUNT_ITU_2]
        channel_count = len(channels)
        channels.extend([0] * (CHANNEL_COUNT_ITU_2 - channel_count))
        parameter = struct.pack(
            "<BBB4IBB53B",
            device_address,
            region,
            bitrate,
            *key_words,
            self.encryption_enabled,
            channel_count,
            *channels,
        )

        header = system_cmd_header_t()
        header.aad.addr_src = 0x01
        header.aad.addr_dest = 0xFF
        header.length = sizeof(system_cmd_header_t) + len(parameter)
        header.command = system_cmd_t.config_set_cmd
        return bytes([START_BYTE]) + bytes(header) + parameter + bytes([STOP_BYTE])

    def _run_configuration(self, com_port, packet):
        error = None
        try:
            with serial.Serial(
                port=com_port,
                baudrate=CONFIGURATION_BAUDRATE,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1,
            ) as connection:
                connection.write(packet)
                connection.flush()
        except Exception as configuration_error:
            error = configuration_error
        self.root.after(0, self._finish_configuration, error)

    def _finish_configuration(self, error):
        self._set_controls_busy(False)
        if error is None:
            self.write_output("Configuration sent successfully.\n")
        else:
            self.write_output(f"Configuration send failed: {error}\n")

    def get_configuration(self):
        try:
            source_address = self.load_address.get()
            radio_address = self.radio_address.get()
        except tk.TclError:
            messagebox.showerror("Configuration error", "Enter valid source and radio addresses.")
            return

        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Configuration error", "Select a COM port first.")
            return

        self._start_operation(
            self._run_with_gui_output,
            (configuration_get, com_port, source_address, radio_address),
        )

    def reset_radio(self):
        try:
            source_address = self.load_address.get()
            radio_address = self.radio_address.get()
        except tk.TclError:
            messagebox.showerror("Reset error", "Enter valid source and radio addresses.")
            return

        com_port = self.com_port.get()
        if not com_port:
            messagebox.showerror("Reset error", "Select a COM port first.")
            return

        self._start_operation(
            self._run_with_gui_output,
            (radio_reset, com_port, source_address, radio_address),
        )

    def write_output(self, text):
        self.output.configure(state="normal")
        if "\r" in text:
            self.output.delete("insert linestart", "insert lineend")
            text = text.replace("\r", "")
        self.output.insert(tk.END, text)
        self.output.see(tk.END)
        self.output.configure(state="disabled")

    def clear_output(self):
        self.output.configure(state="normal")
        self.output.delete("1.0", tk.END)
        self.output.configure(state="disabled")

    def load(self):
        file_path = Path(self.file_path.get())
        com_port = self.com_port.get()
        receive_address = self.load_address.get()
        radio_address = self.radio_address.get()
        load_mode = self.load_mode.get()

        if not self.file_path.get():
            messagebox.showerror("Load error", "Select a file first.")
            return
        if not file_path.is_file():
            messagebox.showerror("Load error", "The selected file does not exist.")
            return
        if not com_port:
            messagebox.showerror("Load error", "Select a COM port first.")
            return

        try:
            self.clear_output()
            self._start_operation(
                self._run_load,
                (load_mode, receive_address, radio_address, file_path, com_port),
            )
        except Exception as error:
            self._finish_load(error)

    def _run_load(self, load_mode, receive_address, radio_address, file_path, com_port):
        error = None
        gui_output = _GuiOutput(self.root, self.write_output)
        try:
            with redirect_stdout(gui_output), redirect_stderr(gui_output):
                if load_mode == "Bootloader":
                    load_via_bl_serial(file_path, com_port)
                else:
                    load_via_app_serial(
                        receive_address, radio_address, file_path, com_port
                    )
        except SystemExit as load_error:
            exit_code = load_error.code if load_error.code is not None else 0
            error = RuntimeError(f"Firmware loader failed with exit code {exit_code}.")
        except Exception as load_error:
            error = load_error
        self.root.after(0, self._finish_load, error)

    def _finish_load(self, error):
        self._set_controls_busy(False)
        if error is None:
            messagebox.showinfo("Load complete", "The firmware was loaded successfully.")
        else:
            messagebox.showerror("Load error", str(error))


class _GuiOutput:
    def __init__(self, root, write_callback):
        self.root = root
        self.write_callback = write_callback

    def write(self, text):
        self.root.after(0, self.write_callback, text)
        return len(text)

    def flush(self):
        pass


if __name__ == "__main__":
    root = tk.Tk()
    icon_path = Path(r"C:\Projects\bridge_0\Design\Graphics\orbweaver.ico")
    if icon_path.is_file():
        try:
            root.iconbitmap(str(icon_path))
        except tk.TclError:
            pass
    app = Configurator(root)
    root.mainloop()