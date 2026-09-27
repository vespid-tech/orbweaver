package com.example.orbweaver

enum class SystemCmd {
    VOID_CMD,
    ECHO_CMD,
    ECHO_RESPONSE_CMD,
    ERROR_CMD,
    SET_FREQ_BAND_CMD,
    CHANNEL_SET_CMD,
    CHANNEL_GET_CMD,
    CHANNEL_CMD,
    BITRATE_SET_CMD,
    BITRATE_GET_CMD,
    BITRATE_CMD,
    SET_CIPHER_KEY_CMD,
    ENCRYPTION_ENABLE_CMD,
    FREQ_HOP_ENABLE_CMD,
    RANDOMIZE_CHANNEL_LIST_CMD,
    CHANNEL_LIST_SET_CMD,
    CHANNEL_LIST_GET_CMD,
    CHANNEL_LIST_CMD,
    CLOCK_SYNC_CMD,
    LED_SET_CMD,
    FLASH_WRITE_CMD,
    FLASH_READ_CMD,
    FLASH_GET_CMD,
    FLASH_DATA_CMD,
    FLASH_ERASE_CMD,
    UART_ENABLE_CMD,
    NETWORK_GATHER_CMD,
    STATUS_REQUEST_CMD,
    STATUS_CMD,
    ASSIGN_ADDR_CMD,
    RADIO_XMIT_CMD,
    BUILD_SUBNET_CMD,
    UART_PEERS_REQUEST_CMD,
    UART_PEERS_CMD,
    SPI_PEERS_REQUEST_CMD,
    SPI_PEERS_CMD,
    TCP_PEERS_CMD,
    RSSI_GET_CMD,
    RSSI_CMD,
    RESET_RADIO_CMD,
    CONFIG_SET_CMD,
    CONFIG_DELETE_CMD,
    FIRMWARE_VER_GET_CMD,
    FIRMWARE_VER_CMD,
    ACK_CMD,
    NAK_CMD,
    RADIO_STATS_GET_CMD,
    RADIO_STATS_CLEAR_CMD,
    RADIO_STATS_CMD,
    TEST_ENDURANCE_CMD,
    FILE_GET_CMD,
    FILE_SET_CMD,
    FILE_TRANSFER_INIT_CMD,
    FILE_TRANSFER_START_CMD,
    FILE_FRAG_REQUEST_CMD,
    FILE_FRAG_CMD,
    FILE_TRANSFER_COMPLETE_CMD,
    MAC_ADDR_GET_CMD,
    MAC_ADDR_CMD,
    BLOB_CMD,
    CMD_MAX_CMD;

    val value: Byte get() = this.ordinal.toByte()

    companion object {
        fun fromByte(value: Byte): SystemCmd {
            val intValue = value.toInt() and 0xFF
            return entries.getOrNull(intValue) ?: VOID_CMD
        }
    }
}

data class Aad(
    val sequence: Byte,
    val addrSrc: Byte,
    val addrDest: Byte
) {
    fun toByteArray(): ByteArray = byteArrayOf(sequence, addrSrc, addrDest)
}

data class SystemCmdHeader(
    val aad: Aad,
    val ttl: Byte,
    val length: Byte,
    val command: SystemCmd
) {
    fun toByteArray(): ByteArray {
        val result = ByteArray(6)
        result[0] = aad.sequence
        result[1] = aad.addrSrc
        result[2] = aad.addrDest
        result[3] = ttl
        result[4] = length
        result[5] = command.value
        return result
    }

    companion object {
        fun fromByteArray(data: ByteArray): SystemCmdHeader {
            require(data.size >= 6) { "Data size must be at least 6 bytes" }
            return SystemCmdHeader(
                aad = Aad(data[0], data[1], data[2]),
                ttl = data[3],
                length = data[4],
                command = SystemCmd.fromByte(data[5])
            )
        }
    }
}
