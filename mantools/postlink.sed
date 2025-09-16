#!/bin/sh

# Crude script to make formatted Postfix man pages clickable.

# If you use a sed(1) command that does not understand POSIX,
# do s/\[\[:<:\]\]/\\</g; s/\[\[:>:\]\]/\\>/g on this script.

exec sed '

	# Glue together words that were broken across line breaks.

    :again
	/-[</bB>]*$/{
	    N
	    b again
	}

	/<[Aa] *[HhNn][RrAa][EeMm][FfEe] *=/{
		p
		d
	}
	/<\/[Aa]>/{
		p
		d
	}
	/"[Hh][Tt][Tt][Pp]:/{
		p
		d
	}
	/<[Tt][Ii][Tt][Ll][Ee]>/{
		p
		d
	}

	# Following block was generated with "makepostconflinks"
	# but hyphenation was added manually.

	/<\/*[Hh][0-9]*>/{
		p
		d
		}
	/<[Aa] [Nm][Aa][Mm][Ee]=/{
		p
		d
		}
	/<[D][T]>/{
		p
		d
		}
	s;[[:<:]]autho[-</bB>]*\n*[ <bB>]*rized_verp_clients[[:>:]];<a href="postconf.5.html#authorized_verp_clients">&</a>;g
	s;[[:<:]]debugger_command[[:>:]];<a href="postconf.5.html#debugger_command">&</a>;g
	s;[[:<:]]2bounce_notice_recipi[-</bB>]*\n*[ <bB>]*ent[[:>:]];<a href="postconf.5.html#2bounce_notice_recipient">&</a>;g
	s;[[:<:]]access_map_reject_code[[:>:]];<a href="postconf.5.html#access_map_reject_code">&</a>;g
	s;[[:<:]]address_verify_default_transport[[:>:]];<a href="postconf.5.html#address_verify_default_transport">&</a>;g
	s;[[:<:]]address_verify_local_transport[[:>:]];<a href="postconf.5.html#address_verify_local_transport">&</a>;g
	s;[[:<:]]address_verify_map[[:>:]];<a href="postconf.5.html#address_verify_map">&</a>;g
	s;[[:<:]]address_verify_negative_cache[[:>:]];<a href="postconf.5.html#address_verify_negative_cache">&</a>;g
	s;[[:<:]]address_verify_negative_expire_time[[:>:]];<a href="postconf.5.html#address_verify_negative_expire_time">&</a>;g
	s;[[:<:]]address_verify_negative_refresh_time[[:>:]];<a href="postconf.5.html#address_verify_negative_refresh_time">&</a>;g
	s;[[:<:]]address_verify_poll_count[[:>:]];<a href="postconf.5.html#address_verify_poll_count">&</a>;g
	s;[[:<:]]address_verify_poll_delay[[:>:]];<a href="postconf.5.html#address_verify_poll_delay">&</a>;g
	s;[[:<:]]address_verify_positive_expire_time[[:>:]];<a href="postconf.5.html#address_verify_positive_expire_time">&</a>;g
	s;[[:<:]]address_verify_positive_refresh_time[[:>:]];<a href="postconf.5.html#address_verify_positive_refresh_time">&</a>;g
	s;[[:<:]]address_verify_relay_transport[[:>:]];<a href="postconf.5.html#address_verify_relay_transport">&</a>;g
	s;[[:<:]]address_verify_relayhost[[:>:]];<a href="postconf.5.html#address_verify_relayhost">&</a>;g
	s;[[:<:]]address_verify_sender[[:>:]];<a href="postconf.5.html#address_verify_sender">&</a>;g
	s;[[:<:]]address_verify_service_name[[:>:]];<a href="postconf.5.html#address_verify_service_name">&</a>;g
	s;[[:<:]]address_verify_transport_maps[[:>:]];<a href="postconf.5.html#address_verify_transport_maps">&</a>;g
	s;[[:<:]]address_verify_virtual_transport[[:>:]];<a href="postconf.5.html#address_verify_virtual_transport">&</a>;g
	s;[[:<:]]alias_database[[:>:]];<a href="postconf.5.html#alias_database">&</a>;g
	s;[[:<:]]alias_maps[[:>:]];<a href="postconf.5.html#alias_maps">&</a>;g
	s;[[:<:]]allow_mail_to_commands[[:>:]];<a href="postconf.5.html#allow_mail_to_commands">&</a>;g
	s;[[:<:]]allow_mail_to_files[[:>:]];<a href="postconf.5.html#allow_mail_to_files">&</a>;g
	s;[[:<:]]allow_min_user[[:>:]];<a href="postconf.5.html#allow_min_user">&</a>;g
	s;[[:<:]]allow_percent_hack[[:>:]];<a href="postconf.5.html#allow_percent_hack">&</a>;g
	s;[[:<:]]allow_untrusted_routing[[:>:]];<a href="postconf.5.html#allow_untrusted_routing">&</a>;g
	s;[[:<:]]alternate_config_directories[[:>:]];<a href="postconf.5.html#alternate_config_directories">&</a>;g
	s;[[:<:]]always_bcc[[:>:]];<a href="postconf.5.html#always_bcc">&</a>;g
	s;[[:<:]]anvil_rate_time_unit[[:>:]];<a href="postconf.5.html#anvil_rate_time_unit">&</a>;g
	s;[[:<:]]append_at_myorigin[[:>:]];<a href="postconf.5.html#append_at_myorigin">&</a>;g
	s;[[:<:]]append_dot_mydomain[[:>:]];<a href="postconf.5.html#append_dot_mydomain">&</a>;g
	s;[[:<:]]application_event_drain_time[[:>:]];<a href="postconf.5.html#application_event_drain_time">&</a>;g
	s;[[:<:]]backwards_bounce_logfile_compatibility[[:>:]];<a href="postconf.5.html#backwards_bounce_logfile_compatibility">&</a>;g
	s;[[:<:]]berkeley_db_create_buffer_size[[:>:]];<a href="postconf.5.html#berkeley_db_create_buffer_size">&</a>;g
	s;[[:<:]]berkeley_db_read_buffer_size[[:>:]];<a href="postconf.5.html#berkeley_db_read_buffer_size">&</a>;g
	s;[[:<:]]best_mx_transport[[:>:]];<a href="postconf.5.html#best_mx_transport">&</a>;g
	s;[[:<:]]biff[[:>:]];<a href="postconf.5.html#biff">&</a>;g
	s;[[:<:]]body_checks[[:>:]];<a href="postconf.5.html#body_checks">&</a>;g
	s;[[:<:]]body_checks_size_limit[[:>:]];<a href="postconf.5.html#body_checks_size_limit">&</a>;g
	s;[[:<:]]bounce_notice_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#bounce_notice_recipient">&</a>;g
	s;[[:<:]]bounce_queue_lifetime[[:>:]];<a href="postconf.5.html#bounce_queue_lifetime">&</a>;g
	s;[[:<:]]bounce_service_name[[:>:]];<a href="postconf.5.html#bounce_service_name">&</a>;g
	s;[[:<:]]bounce_size_limit[[:>:]];<a href="postconf.5.html#bounce_size_limit">&</a>;g
	s;[[:<:]]broken_sasl_auth_clients[[:>:]];<a href="postconf.5.html#broken_sasl_auth_clients">&</a>;g
	s;[[:<:]]canonical_maps[[:>:]];<a href="postconf.5.html#canonical_maps">&</a>;g
	s;[[:<:]]cleanup_service_name[[:>:]];<a href="postconf.5.html#cleanup_service_name">&</a>;g
	s;[[:<:]]anvil_status_update_time[[:>:]];<a href="postconf.5.html#anvil_status_update_time">&</a>;g
	s;[[:<:]]command_directory[[:>:]];<a href="postconf.5.html#command_directory">&</a>;g
	s;[[:<:]]command_expan[-</bB>]*\n* *[<bB>]*sion_filter[[:>:]];<a href="postconf.5.html#command_expansion_filter">&</a>;g
	s;[[:<:]]command_time_limit[[:>:]];<a href="postconf.5.html#command_time_limit">&</a>;g
	s;[[:<:]]config_direc[-</bB>]*\n*[ <bB>]*tory[[:>:]];<a href="postconf.5.html#config_directory">&</a>;g
	s;[[:<:]]con[-</bB>]*\n*[ <bB>]*tent_filter[[:>:]];<a href="postconf.5.html#content_filter">&</a>;g
	s;[[:<:]]daemon_directory[[:>:]];<a href="postconf.5.html#daemon_directory">&</a>;g
	s;[[:<:]]daemon_timeout[[:>:]];<a href="postconf.5.html#daemon_timeout">&</a>;g
	s;[[:<:]]debug_peer_level[[:>:]];<a href="postconf.5.html#debug_peer_level">&</a>;g
	s;[[:<:]]debug_peer_list[[:>:]];<a href="postconf.5.html#debug_peer_list">&</a>;g
	s;[[:<:]]default_database_type[[:>:]];<a href="postconf.5.html#default_database_type">&</a>;g
	s;[[:<:]]default_deliv[-</Bb>]*\n* *[<Bb>]*ery_slot_cost[[:>:]];<a href="postconf.5.html#default_delivery_slot_cost">&</a>;g
	s;[[:<:]]default_deliv[-</Bb>]*\n* *[<Bb>]*ery_slot_discount[[:>:]];<a href="postconf.5.html#default_delivery_slot_discount">&</a>;g
	s;[[:<:]]default_deliv[-</Bb>]*\n* *[<Bb>]*ery_slot_loan[[:>:]];<a href="postconf.5.html#default_delivery_slot_loan">&</a>;g
	s;[[:<:]]default_destina[-</Bb>]*\n* *[<Bb>]*tion_concurrency_limit[[:>:]];<a href="postconf.5.html#default_destination_concurrency_limit">&</a>;g
	s;[[:<:]]default_destina[-</Bb>]*\n* *[<Bb>]*tion_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#default_destination_recipient_limit">&</a>;g
	s;[[:<:]]default_extra_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#default_extra_recipient_limit">&</a>;g
	s;[[:<:]]default_minimum_deliv[-</Bb>]*\n* *[<Bb>]*ery_slots[[:>:]];<a href="postconf.5.html#default_minimum_delivery_slots">&</a>;g
	s;[[:<:]]default_privs[[:>:]];<a href="postconf.5.html#default_privs">&</a>;g
	s;[[:<:]]default_process_limit[[:>:]];<a href="postconf.5.html#default_process_limit">&</a>;g
	s;[[:<:]]default_rbl_reply[[:>:]];<a href="postconf.5.html#default_rbl_reply">&</a>;g
	s;[[:<:]]default_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#default_recipient_limit">&</a>;g
	s;[[:<:]]default_transport[[:>:]];<a href="postconf.5.html#default_transport">&</a>;g
	s;[[:<:]]default_verp_delimiters[[:>:]];<a href="postconf.5.html#default_verp_delimiters">&</a>;g
	s;[[:<:]]defer_code[[:>:]];<a href="postconf.5.html#defer_code">&</a>;g
	s;[[:<:]]defer_service_name[[:>:]];<a href="postconf.5.html#defer_service_name">&</a>;g
	s;[[:<:]]defer_transports[[:>:]];<a href="postconf.5.html#defer_transports">&</a>;g
	s;[[:<:]]delay_notice_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#delay_notice_recipient">&</a>;g
	s;[[:<:]]delay_warning_time[[:>:]];<a href="postconf.5.html#delay_warning_time">&</a>;g
	s;[[:<:]]deliver_lock_attempts[[:>:]];<a href="postconf.5.html#deliver_lock_attempts">&</a>;g
	s;[[:<:]]deliver_lock_delay[[:>:]];<a href="postconf.5.html#deliver_lock_delay">&</a>;g
	s;[[:<:]]disable_dns_lookups[[:>:]];<a href="postconf.5.html#disable_dns_lookups">&</a>;g
	s;[[:<:]]disable_mime_input_processing[[:>:]];<a href="postconf.5.html#disable_mime_input_processing">&</a>;g
	s;[[:<:]]disable_mime_output_conversion[[:>:]];<a href="postconf.5.html#disable_mime_output_conversion">&</a>;g
	s;[[:<:]]disable_verp_bounces[[:>:]];<a href="postconf.5.html#disable_verp_bounces">&</a>;g
	s;[[:<:]]disable_vrfy_command[[:>:]];<a href="postconf.5.html#disable_vrfy_command">&</a>;g
	s;[[:<:]]dont_remove[[:>:]];<a href="postconf.5.html#dont_remove">&</a>;g
	s;[[:<:]]double_bounce_sender[[:>:]];<a href="postconf.5.html#double_bounce_sender">&</a>;g
	s;[[:<:]]dupli[-</bB>]*\n* *[<bB>]*cate_filter_limit[[:>:]];<a href="postconf.5.html#duplicate_filter_limit">&</a>;g
	s;[[:<:]]empty_address_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#empty_address_recipient">&</a>;g
	s;[[:<:]]enable_original_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#enable_original_recipient">&</a>;g
	s;[[:<:]]error_notice_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#error_notice_recipient">&</a>;g
	s;[[:<:]]error_service_name[[:>:]];<a href="postconf.5.html#error_service_name">&</a>;g
	s;[[:<:]]expand_owner_alias[[:>:]];<a href="postconf.5.html#expand_owner_alias">&</a>;g
	s;[[:<:]]export_environment[[:>:]];<a href="postconf.5.html#export_environment">&</a>;g
	s;[[:<:]]fallback_relay[[:>:]];<a href="postconf.5.html#fallback_relay">&</a>;g
	s;[[:<:]]fallback_transport[[:>:]];<a href="postconf.5.html#fallback_transport">&</a>;g
	s;[[:<:]]fast_flush_domains[[:>:]];<a href="postconf.5.html#fast_flush_domains">&</a>;g
	s;[[:<:]]fast_flush_purge_time[[:>:]];<a href="postconf.5.html#fast_flush_purge_time">&</a>;g
	s;[[:<:]]fast_flush_refresh_time[[:>:]];<a href="postconf.5.html#fast_flush_refresh_time">&</a>;g
	s;[[:<:]]fault_injection_code[[:>:]];<a href="postconf.5.html#fault_injection_code">&</a>;g
	s;[[:<:]]flush_service_name[[:>:]];<a href="postconf.5.html#flush_service_name">&</a>;g
	s;[[:<:]]fork_attempts[[:>:]];<a href="postconf.5.html#fork_attempts">&</a>;g
	s;[[:<:]]fork_delay[[:>:]];<a href="postconf.5.html#fork_delay">&</a>;g
	s;[[:<:]]forward_expan[-</bB>]*\n* *[<bB>]*sion_filter[[:>:]];<a href="postconf.5.html#forward_expansion_filter">&</a>;g
	s;[[:<:]]for[-</bB>]*\n* *[<bB>]*ward_path[[:>:]];<a href="postconf.5.html#forward_path">&</a>;g
	s;[[:<:]]hash_queue_depth[[:>:]];<a href="postconf.5.html#hash_queue_depth">&</a>;g
	s;[[:<:]]hash_queue_names[[:>:]];<a href="postconf.5.html#hash_queue_names">&</a>;g
	s;[[:<:]]header_address_token_limit[[:>:]];<a href="postconf.5.html#header_address_token_limit">&</a>;g
	s;[[:<:]]header_checks[[:>:]];<a href="postconf.5.html#header_checks">&</a>;g
	s;[[:<:]]header_size_limit[[:>:]];<a href="postconf.5.html#header_size_limit">&</a>;g
	s;[[:<:]]helpful_warnings[[:>:]];<a href="postconf.5.html#helpful_warnings">&</a>;g
	s;[[:<:]]home_mailbox[[:>:]];<a href="postconf.5.html#home_mailbox">&</a>;g
	s;[[:<:]]hopcount_limit[[:>:]];<a href="postconf.5.html#hopcount_limit">&</a>;g
	s;[[:<:]]html_direc[-</bB>]*\n*[ <bB>]*tory[[:>:]];<a href="postconf.5.html#html_directory">&</a>;g
	s;[[:<:]]ignore_mx_lookup_error[[:>:]];<a href="postconf.5.html#ignore_mx_lookup_error">&</a>;g
	s;[[:<:]]import_environment[[:>:]];<a href="postconf.5.html#import_environment">&</a>;g
	s;[[:<:]]in_flow_delay[[:>:]];<a href="postconf.5.html#in_flow_delay">&</a>;g
	s;[[:<:]]inet_interfaces[[:>:]];<a href="postconf.5.html#inet_interfaces">&</a>;g
	s;[[:<:]]initial_destination_concurrency[[:>:]];<a href="postconf.5.html#initial_destination_concurrency">&</a>;g
	s;[[:<:]]invalid_hostname_reject_code[[:>:]];<a href="postconf.5.html#invalid_hostname_reject_code">&</a>;g
	s;[[:<:]]ipc_idle[[:>:]];<a href="postconf.5.html#ipc_idle">&</a>;g
	s;[[:<:]]ipc_timeout[[:>:]];<a href="postconf.5.html#ipc_timeout">&</a>;g
	s;[[:<:]]ipc_ttl[[:>:]];<a href="postconf.5.html#ipc_ttl">&</a>;g
	s;[[:<:]]line_length_limit[[:>:]];<a href="postconf.5.html#line_length_limit">&</a>;g
	s;[[:<:]]lmtp_cache_connection[[:>:]];<a href="postconf.5.html#lmtp_cache_connection">&</a>;g
	s;[[:<:]]lmtp_connect_timeout[[:>:]];<a href="postconf.5.html#lmtp_connect_timeout">&</a>;g
	s;[[:<:]]lmtp_data_done_timeout[[:>:]];<a href="postconf.5.html#lmtp_data_done_timeout">&</a>;g
	s;[[:<:]]lmtp_data_init_timeout[[:>:]];<a href="postconf.5.html#lmtp_data_init_timeout">&</a>;g
	s;[[:<:]]lmtp_data_xfer_timeout[[:>:]];<a href="postconf.5.html#lmtp_data_xfer_timeout">&</a>;g
	s;[[:<:]]lmtp_lhlo_timeout[[:>:]];<a href="postconf.5.html#lmtp_lhlo_timeout">&</a>;g
	s;[[:<:]]lmtp_mail_timeout[[:>:]];<a href="postconf.5.html#lmtp_mail_timeout">&</a>;g
	s;[[:<:]]lmtp_quit_timeout[[:>:]];<a href="postconf.5.html#lmtp_quit_timeout">&</a>;g
	s;[[:<:]]lmtp_rcpt_timeout[[:>:]];<a href="postconf.5.html#lmtp_rcpt_timeout">&</a>;g
	s;[[:<:]]lmtp_rset_timeout[[:>:]];<a href="postconf.5.html#lmtp_rset_timeout">&</a>;g
	s;[[:<:]]lmtp_sasl_auth_enable[[:>:]];<a href="postconf.5.html#lmtp_sasl_auth_enable">&</a>;g
	s;[[:<:]]lmtp_sasl_password_maps[[:>:]];<a href="postconf.5.html#lmtp_sasl_password_maps">&</a>;g
	s;[[:<:]]lmtp_sasl_security_options[[:>:]];<a href="postconf.5.html#lmtp_sasl_security_options">&</a>;g
	s;[[:<:]]lmtp_send_xforward_command[[:>:]];<a href="postconf.5.html#lmtp_send_xforward_command">&</a>;g
	s;[[:<:]]lmtp_skip_quit_response[[:>:]];<a href="postconf.5.html#lmtp_skip_quit_response">&</a>;g
	s;[[:<:]]lmtp_tcp_port[[:>:]];<a href="postconf.5.html#lmtp_tcp_port">&</a>;g
	s;[[:<:]]lmtp_xforward_timeout[[:>:]];<a href="postconf.5.html#lmtp_xforward_timeout">&</a>;g
	s;[[:<:]]local_command_shell[[:>:]];<a href="postconf.5.html#local_command_shell">&</a>;g
	s;[[:<:]]local_destination_concurrency_limit[[:>:]];<a href="postconf.5.html#local_destination_concurrency_limit">&</a>;g
	s;[[:<:]]local_destination_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#local_destination_recipient_limit">&</a>;g
	s;[[:<:]]local_recip[-</bB>]*\n* *[<bB>]*ient_maps[[:>:]];<a href="postconf.5.html#local_recipient_maps">&</a>;g
	s;[[:<:]]local_transport[[:>:]];<a href="postconf.5.html#local_transport">&</a>;g
	s;[[:<:]]luser_relay[[:>:]];<a href="postconf.5.html#luser_relay">&</a>;g
	s;[[:<:]]mail_name[[:>:]];<a href="postconf.5.html#mail_name">&</a>;g
	s;[[:<:]]mail_owner[[:>:]];<a href="postconf.5.html#mail_owner">&</a>;g
	s;[[:<:]]mail_release_date[[:>:]];<a href="postconf.5.html#mail_release_date">&</a>;g
	s;[[:<:]]mail_spool_direc[-</bB>]*\n* *[<bB>]*tory[[:>:]];<a href="postconf.5.html#mail_spool_directory">&</a>;g
	s;[[:<:]]mail_version[[:>:]];<a href="postconf.5.html#mail_version">&</a>;g
	s;[[:<:]]mail[-</bB>]*\n* *[<bB>]*box_command[[:>:]];<a href="postconf.5.html#mailbox_command">&</a>;g
	s;[[:<:]]mail[-</bB>]*\n* *[<bB>]*box_command_maps[[:>:]];<a href="postconf.5.html#mailbox_command_maps">&</a>;g
	s;[[:<:]]mail[-</bB>]*\n* *[<bB>]*box_deliv[-</Bb>]*\n* *[<Bb>]*ery_lock[[:>:]];<a href="postconf.5.html#mailbox_delivery_lock">&</a>;g
	s;[[:<:]]mail[-</bB>]*\n* *[<bB>]*box_size_limit[[:>:]];<a href="postconf.5.html#mailbox_size_limit">&</a>;g
	s;[[:<:]]mail[-</bB>]*\n* *[<bB>]*box_transport[[:>:]];<a href="postconf.5.html#mailbox_transport">&</a>;g
	s;[[:<:]]mailq_path[[:>:]];<a href="postconf.5.html#mailq_path">&</a>;g
	s;[[:<:]]manpage_directory[[:>:]];<a href="postconf.5.html#manpage_directory">&</a>;g
	s;[[:<:]]maps_rbl_domains[[:>:]];<a href="postconf.5.html#maps_rbl_domains">&</a>;g
	s;[[:<:]]maps_rbl_reject_code[[:>:]];<a href="postconf.5.html#maps_rbl_reject_code">&</a>;g
	s;[[:<:]]masquerade_classes[[:>:]];<a href="postconf.5.html#masquerade_classes">&</a>;g
	s;[[:<:]]masquerade_domains[[:>:]];<a href="postconf.5.html#masquerade_domains">&</a>;g
	s;[[:<:]]masquerade_exceptions[[:>:]];<a href="postconf.5.html#masquerade_exceptions">&</a>;g
	s;[[:<:]]max_idle[[:>:]];<a href="postconf.5.html#max_idle">&</a>;g
	s;[[:<:]]max_use[[:>:]];<a href="postconf.5.html#max_use">&</a>;g
	s;[[:<:]]maxi[-</bB>]*\n*[ <bB>]*mal_backoff_time[[:>:]];<a href="postconf.5.html#maximal_backoff_time">&</a>;g
	s;[[:<:]]maxi[-</bB>]*\n*[ <bB>]*mal_queue_lifetime[[:>:]];<a href="postconf.5.html#maximal_queue_lifetime">&</a>;g
	s;[[:<:]]message_size_limit[[:>:]];<a href="postconf.5.html#message_size_limit">&</a>;g
	s;[[:<:]]mime_boundary_length_limit[[:>:]];<a href="postconf.5.html#mime_boundary_length_limit">&</a>;g
	s;[[:<:]]mime_header_checks[[:>:]];<a href="postconf.5.html#mime_header_checks">&</a>;g
	s;[[:<:]]mime_nesting_limit[[:>:]];<a href="postconf.5.html#mime_nesting_limit">&</a>;g
	s;[[:<:]]minimal_backoff_time[[:>:]];<a href="postconf.5.html#minimal_backoff_time">&</a>;g
	s;[[:<:]]multi_recip[-</bB>]*\n* *[<bB>]*ient_bounce_reject_code[[:>:]];<a href="postconf.5.html#multi_recipient_bounce_reject_code">&</a>;g
	s;[[:<:]]mydes[-</bB>]*\n*[ <bB>]*tina[-</bB>]*\n*[ <bB>]*tion[[:>:]];<a href="postconf.5.html#mydestination">&</a>;g
	s;[[:<:]]mydomain[[:>:]];<a href="postconf.5.html#mydomain">&</a>;g
	s;[[:<:]]myhostname[[:>:]];<a href="postconf.5.html#myhostname">&</a>;g
	s;[[:<:]]mynetworks[[:>:]];<a href="postconf.5.html#mynetworks">&</a>;g
	s;[[:<:]]mynetworks_style[[:>:]];<a href="postconf.5.html#mynetworks_style">&</a>;g
	s;[[:<:]]myorigin[[:>:]];<a href="postconf.5.html#myorigin">&</a>;g
	s;[[:<:]]nested_header_checks[[:>:]];<a href="postconf.5.html#nested_header_checks">&</a>;g
	s;[[:<:]]newaliases_path[[:>:]];<a href="postconf.5.html#newaliases_path">&</a>;g
	s;[[:<:]]non_fqdn_reject_code[[:>:]];<a href="postconf.5.html#non_fqdn_reject_code">&</a>;g
	s;[[:<:]]notify_classes[[:>:]];<a href="postconf.5.html#notify_classes">&</a>;g
	s;[[:<:]]owner_request_special[[:>:]];<a href="postconf.5.html#owner_request_special">&</a>;g
	s;[[:<:]]parent_domain_matches_subdomains[[:>:]];<a href="postconf.5.html#parent_domain_matches_subdomains">&</a>;g
	s;[[:<:]]permit_mx_backup_networks[[:>:]];<a href="postconf.5.html#permit_mx_backup_networks">&</a>;g
	s;[[:<:]]pickup_service_name[[:>:]];<a href="postconf.5.html#pickup_service_name">&</a>;g
	s;[[:<:]]prepend_delivered_header[[:>:]];<a href="postconf.5.html#prepend_delivered_header">&</a>;g
	s;[[:<:]]process_id[[:>:]];<a href="postconf.5.html#process_id">&</a>;g
	s;[[:<:]]process_id_directory[[:>:]];<a href="postconf.5.html#process_id_directory">&</a>;g
	s;[[:<:]]process_name[[:>:]];<a href="postconf.5.html#process_name">&</a>;g
	s;[[:<:]]propagate_unmatched_extensions[[:>:]];<a href="postconf.5.html#propagate_unmatched_extensions">&</a>;g
	s;[[:<:]]proxy_interfaces[[:>:]];<a href="postconf.5.html#proxy_interfaces">&</a>;g
	s;[[:<:]]proxy_read_maps[[:>:]];<a href="postconf.5.html#proxy_read_maps">&</a>;g
	s;[[:<:]]qmgr_clog_warn_time[[:>:]];<a href="postconf.5.html#qmgr_clog_warn_time">&</a>;g
	s;[[:<:]]qmgr_fudge_factor[[:>:]];<a href="postconf.5.html#qmgr_fudge_factor">&</a>;g
	s;[[:<:]]qmgr_message_active_limit[[:>:]];<a href="postconf.5.html#qmgr_message_active_limit">&</a>;g
	s;[[:<:]]qmgr_message_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#qmgr_message_recipient_limit">&</a>;g
	s;[[:<:]]qmgr_message_recip[-</bB>]*\n* *[<bB>]*ient_minimum[[:>:]];<a href="postconf.5.html#qmgr_message_recipient_minimum">&</a>;g
	s;[[:<:]]qmqpd_authorized_clients[[:>:]];<a href="postconf.5.html#qmqpd_authorized_clients">&</a>;g
	s;[[:<:]]qmqpd_error_delay[[:>:]];<a href="postconf.5.html#qmqpd_error_delay">&</a>;g
	s;[[:<:]]qmqpd_timeout[[:>:]];<a href="postconf.5.html#qmqpd_timeout">&</a>;g
	s;[[:<:]]queue_directory[[:>:]];<a href="postconf.5.html#queue_directory">&</a>;g
	s;[[:<:]]queue_file_attribute_count_limit[[:>:]];<a href="postconf.5.html#queue_file_attribute_count_limit">&</a>;g
	s;[[:<:]]queue_minfree[[:>:]];<a href="postconf.5.html#queue_minfree">&</a>;g
	s;[[:<:]]queue_run_delay[[:>:]];<a href="postconf.5.html#queue_run_delay">&</a>;g
	s;[[:<:]]queue_service_name[[:>:]];<a href="postconf.5.html#queue_service_name">&</a>;g
	s;[[:<:]]rbl_reply_maps[[:>:]];<a href="postconf.5.html#rbl_reply_maps">&</a>;g
	s;[[:<:]]readme_directory[[:>:]];<a href="postconf.5.html#readme_directory">&</a>;g
	s;[[:<:]]receive_override_options[[:>:]];<a href="postconf.5.html#receive_override_options">&</a>;g
	s;[[:<:]]no_unknown_recip[-</bB>]*\n* *[<bB>]*ient_checks[[:>:]];<a href="postconf.5.html#no_unknown_recipient_checks">&</a>;g
	s;[[:<:]]no_address_mappings[[:>:]];<a href="postconf.5.html#no_address_mappings">&</a>;g
	s;[[:<:]]no_header_body_checks[[:>:]];<a href="postconf.5.html#no_header_body_checks">&</a>;g
	s;[[:<:]]recip[-</bB>]*\n* *[<bB>]*ient_bcc_maps[[:>:]];<a href="postconf.5.html#recipient_bcc_maps">&</a>;g
	s;[[:<:]]recip[-</bB>]*\n* *[<bB>]*ient_canonical_maps[[:>:]];<a href="postconf.5.html#recipient_canonical_maps">&</a>;g
	s;[[:<:]]recip[-</bB>]*\n* *[<bB>]*ient_delim[-</bB>]*\n* *[<bB>]*iter[[:>:]];<a href="postconf.5.html#recipient_delimiter">&<\/a>;g
	s;[[:<:]]reject_code[[:>:]];<a href="postconf.5.html#reject_code">&</a>;g
	s;[[:<:]]relay_domains[[:>:]];<a href="postconf.5.html#relay_domains">&</a>;g
	s;[[:<:]]relay_domains_reject_code[[:>:]];<a href="postconf.5.html#relay_domains_reject_code">&</a>;g
	s;[[:<:]]relay_recipi[-</bB>]*\n*[ <bB>]*ent_maps[[:>:]];<a href="postconf.5.html#relay_recipient_maps">&</a>;g
	s;[[:<:]]relay_transport[[:>:]];<a href="postconf.5.html#relay_transport">&</a>;g
	s;[[:<:]]relayhost[[:>:]];<a href="postconf.5.html#relayhost">&</a>;g
	s;[[:<:]]relocated_maps[[:>:]];<a href="postconf.5.html#relocated_maps">&</a>;g
	s;[[:<:]]require_home_directory[[:>:]];<a href="postconf.5.html#require_home_directory">&</a>;g
	s;[[:<:]]resolve_dequoted_address[[:>:]];<a href="postconf.5.html#resolve_dequoted_address">&</a>;g
	s;[[:<:]]rewrite_service_name[[:>:]];<a href="postconf.5.html#rewrite_service_name">&</a>;g
	s;[[:<:]]sample_directory[[:>:]];<a href="postconf.5.html#sample_directory">&</a>;g
	s;[[:<:]]sender_based_routing[[:>:]];<a href="postconf.5.html#sender_based_routing">&</a>;g
	s;[[:<:]]sender_bcc_maps[[:>:]];<a href="postconf.5.html#sender_bcc_maps">&</a>;g
	s;[[:<:]]sender_canonical_maps[[:>:]];<a href="postconf.5.html#sender_canonical_maps">&</a>;g
	s;[[:<:]]sendmail_path[[:>:]];<a href="postconf.5.html#sendmail_path">&</a>;g
	s;[[:<:]]service_throttle_time[[:>:]];<a href="postconf.5.html#service_throttle_time">&</a>;g
	s;[[:<:]]setgid_group[[:>:]];<a href="postconf.5.html#setgid_group">&</a>;g
	s;[[:<:]]show_user_unknown_table_name[[:>:]];<a href="postconf.5.html#show_user_unknown_table_name">&</a>;g
	s;[[:<:]]showq_service_name[[:>:]];<a href="postconf.5.html#showq_service_name">&</a>;g
	s;[[:<:]]smtp_always_send_ehlo[[:>:]];<a href="postconf.5.html#smtp_always_send_ehlo">&</a>;g
	s;[[:<:]]smtp_bind_address[[:>:]];<a href="postconf.5.html#smtp_bind_address">&</a>;g
	s;[[:<:]]smtp_connect_timeout[[:>:]];<a href="postconf.5.html#smtp_connect_timeout">&</a>;g
	s;[[:<:]]smtp_data_done_timeout[[:>:]];<a href="postconf.5.html#smtp_data_done_timeout">&</a>;g
	s;[[:<:]]smtp_data_init_timeout[[:>:]];<a href="postconf.5.html#smtp_data_init_timeout">&</a>;g
	s;[[:<:]]smtp_data_xfer_timeout[[:>:]];<a href="postconf.5.html#smtp_data_xfer_timeout">&</a>;g
	s;[[:<:]]smtp_defer_if_no_mx_address_found[[:>:]];<a href="postconf.5.html#smtp_defer_if_no_mx_address_found">&</a>;g
	s;[[:<:]]lmtp_destination_concurrency_limit[[:>:]];<a href="postconf.5.html#lmtp_destination_concurrency_limit">&</a>;g
	s;[[:<:]]lmtp_destination_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#lmtp_destination_recipient_limit">&</a>;g
	s;[[:<:]]relay_destination_concurrency_limit[[:>:]];<a href="postconf.5.html#relay_destination_concurrency_limit">&</a>;g
	s;[[:<:]]relay_destination_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#relay_destination_recipient_limit">&</a>;g
	s;[[:<:]]resolve_null_domain[[:>:]];<a href="postconf.5.html#resolve_null_domain">&</a>;g
	s;[[:<:]]smtp_destination_concurrency_limit[[:>:]];<a href="postconf.5.html#smtp_destination_concurrency_limit">&</a>;g
	s;[[:<:]]smtp_destination_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#smtp_destination_recipient_limit">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_destination_concurrency_limit[[:>:]];<a href="postconf.5.html#virtual_destination_concurrency_limit">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_destination_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#virtual_destination_recipient_limit">&</a>;g
	s;[[:<:]]smtp_helo_name[[:>:]];<a href="postconf.5.html#smtp_helo_name">&</a>;g
	s;[[:<:]]smtp_helo_timeout[[:>:]];<a href="postconf.5.html#smtp_helo_timeout">&</a>;g
	s;[[:<:]]smtp_host_lookup[[:>:]];<a href="postconf.5.html#smtp_host_lookup">&</a>;g
	s;[[:<:]]smtp_line_length_limit[[:>:]];<a href="postconf.5.html#smtp_line_length_limit">&</a>;g
	s;[[:<:]]smtp_mail_timeout[[:>:]];<a href="postconf.5.html#smtp_mail_timeout">&</a>;g
	s;[[:<:]]smtp_mx_address_limit[[:>:]];<a href="postconf.5.html#smtp_mx_address_limit">&</a>;g
	s;[[:<:]]smtp_mx_session_limit[[:>:]];<a href="postconf.5.html#smtp_mx_session_limit">&</a>;g
	s;[[:<:]]smtp_never_send_ehlo[[:>:]];<a href="postconf.5.html#smtp_never_send_ehlo">&</a>;g
	s;[[:<:]]smtp_pix_workaround_delay_time[[:>:]];<a href="postconf.5.html#smtp_pix_workaround_delay_time">&</a>;g
	s;[[:<:]]smtp_pix_workaround_threshold_time[[:>:]];<a href="postconf.5.html#smtp_pix_workaround_threshold_time">&</a>;g
	s;[[:<:]]smtp_quit_timeout[[:>:]];<a href="postconf.5.html#smtp_quit_timeout">&</a>;g
	s;[[:<:]]smtp_quote_rfc821_envelope[[:>:]];<a href="postconf.5.html#smtp_quote_rfc821_envelope">&</a>;g
	s;[[:<:]]smtp_randomize_addresses[[:>:]];<a href="postconf.5.html#smtp_randomize_addresses">&</a>;g
	s;[[:<:]]smtp_rcpt_timeout[[:>:]];<a href="postconf.5.html#smtp_rcpt_timeout">&</a>;g
	s;[[:<:]]smtp_rset_timeout[[:>:]];<a href="postconf.5.html#smtp_rset_timeout">&</a>;g
	s;[[:<:]]smtp_sasl_auth_enable[[:>:]];<a href="postconf.5.html#smtp_sasl_auth_enable">&</a>;g
	s;[[:<:]]smtp_sasl_password_maps[[:>:]];<a href="postconf.5.html#smtp_sasl_password_maps">&</a>;g
	s;[[:<:]]smtp_sasl_security_options[[:>:]];<a href="postconf.5.html#smtp_sasl_security_options">&</a>;g
	s;[[:<:]]smtp_send_xforward_command[[:>:]];<a href="postconf.5.html#smtp_send_xforward_command">&</a>;g
	s;[[:<:]]smtp_skip_4xx_greeting[[:>:]];<a href="postconf.5.html#smtp_skip_4xx_greeting">&</a>;g
	s;[[:<:]]smtp_skip_5xx_greeting[[:>:]];<a href="postconf.5.html#smtp_skip_5xx_greeting">&</a>;g
	s;[[:<:]]smtp_skip_quit_response[[:>:]];<a href="postconf.5.html#smtp_skip_quit_response">&</a>;g
	s;[[:<:]]smtp_xforward_timeout[[:>:]];<a href="postconf.5.html#smtp_xforward_timeout">&</a>;g
	s;[[:<:]]smtpd_autho[-</bB>]*\n*[ <bB>]*rized_verp_clients[[:>:]];<a href="postconf.5.html#smtpd_authorized_verp_clients">&</a>;g
	s;[[:<:]]smtpd_autho[-</bB>]*\n*[ <bB>]*rized_xclient_hosts[[:>:]];<a href="postconf.5.html#smtpd_authorized_xclient_hosts">&</a>;g
	s;[[:<:]]smtpd_autho[-</bB>]*\n*[ <bB>]*rized_xforward_hosts[[:>:]];<a href="postconf.5.html#smtpd_authorized_xforward_hosts">&</a>;g
	s;[[:<:]]smtpd_banner[[:>:]];<a href="postconf.5.html#smtpd_banner">&</a>;g
	s;[[:<:]]smtpd_client_connection_count_limit[[:>:]];<a href="postconf.5.html#smtpd_client_connection_count_limit">&</a>;g
	s;[[:<:]]smtpd_client_connection_limit_exceptions[[:>:]];<a href="postconf.5.html#smtpd_client_connection_limit_exceptions">&</a>;g
	s;[[:<:]]smtpd_client_connection_rate_limit[[:>:]];<a href="postconf.5.html#smtpd_client_connection_rate_limit">&</a>;g
	s;[[:<:]]smtpd_client_restrictions[[:>:]];<a href="postconf.5.html#smtpd_client_restrictions">&</a>;g
	s;[[:<:]]smtpd_data_restrictions[[:>:]];<a href="postconf.5.html#smtpd_data_restrictions">&</a>;g
	s;[[:<:]]smtpd_delay_reject[[:>:]];<a href="postconf.5.html#smtpd_delay_reject">&</a>;g
	s;[[:<:]]smtpd_error_sleep_time[[:>:]];<a href="postconf.5.html#smtpd_error_sleep_time">&</a>;g
	s;[[:<:]]smtpd_etrn_restrictions[[:>:]];<a href="postconf.5.html#smtpd_etrn_restrictions">&</a>;g
	s;[[:<:]]smtpd_expansion_filter[[:>:]];<a href="postconf.5.html#smtpd_expansion_filter">&</a>;g
	s;[[:<:]]smtpd_hard_error_limit[[:>:]];<a href="postconf.5.html#smtpd_hard_error_limit">&</a>;g
	s;[[:<:]]smtpd_helo_required[[:>:]];<a href="postconf.5.html#smtpd_helo_required">&</a>;g
	s;[[:<:]]smtpd_helo_restrictions[[:>:]];<a href="postconf.5.html#smtpd_helo_restrictions">&</a>;g
	s;[[:<:]]smtpd_history_flush_threshold[[:>:]];<a href="postconf.5.html#smtpd_history_flush_threshold">&</a>;g
	s;[[:<:]]smtpd_junk_command_limit[[:>:]];<a href="postconf.5.html#smtpd_junk_command_limit">&</a>;g
	s;[[:<:]]smtpd_noop_commands[[:>:]];<a href="postconf.5.html#smtpd_noop_commands">&</a>;g
	s;[[:<:]]smtpd_null_access_lookup_key[[:>:]];<a href="postconf.5.html#smtpd_null_access_lookup_key">&</a>;g
	s;[[:<:]]smtpd_recipient_overshoot_limit[[:>:]];<a href="postconf.5.html#smtpd_recipient_overshoot_limit">&</a>;g
	s;[[:<:]]smtpd_policy_service_max_idle[[:>:]];<a href="postconf.5.html#smtpd_policy_service_max_idle">&</a>;g
	s;[[:<:]]smtpd_policy_service_max_ttl[[:>:]];<a href="postconf.5.html#smtpd_policy_service_max_ttl">&</a>;g
	s;[[:<:]]smtpd_policy_service_timeout[[:>:]];<a href="postconf.5.html#smtpd_policy_service_timeout">&</a>;g
	s;[[:<:]]smtpd_proxy_ehlo[[:>:]];<a href="postconf.5.html#smtpd_proxy_ehlo">&</a>;g
	s;[[:<:]]smtpd_proxy_filter[[:>:]];<a href="postconf.5.html#smtpd_proxy_filter">&</a>;g
	s;[[:<:]]smtpd_proxy_timeout[[:>:]];<a href="postconf.5.html#smtpd_proxy_timeout">&</a>;g
	s;[[:<:]]smtpd_recip[-</bB>]*\n* *[<bB>]*ient_limit[[:>:]];<a href="postconf.5.html#smtpd_recipient_limit">&</a>;g
	s;[[:<:]]smtpd_recip[-</bB>]*\n* *[<bB>]*ient_restrictions[[:>:]];<a href="postconf.5.html#smtpd_recipient_restrictions">&</a>;g
	s;[[:<:]]smtpd_reject_unlisted_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#smtpd_reject_unlisted_recipient">&</a>;g
	s;[[:<:]]smtpd_reject_unlisted_sender[[:>:]];<a href="postconf.5.html#smtpd_reject_unlisted_sender">&</a>;g
	s;[[:<:]]smtpd_restriction_classes[[:>:]];<a href="postconf.5.html#smtpd_restriction_classes">&</a>;g
	s;[[:<:]]smtpd_sasl_application_name[[:>:]];<a href="postconf.5.html#smtpd_sasl_application_name">&</a>;g
	s;[[:<:]]smtpd_sasl_auth_enable[[:>:]];<a href="postconf.5.html#smtpd_sasl_auth_enable">&</a>;g
	s;[[:<:]]smtpd_sasl_exceptions_networks[[:>:]];<a href="postconf.5.html#smtpd_sasl_exceptions_networks">&</a>;g
	s;[[:<:]]smtpd_sasl_local_domain[[:>:]];<a href="postconf.5.html#smtpd_sasl_local_domain">&</a>;g
	s;[[:<:]]smtpd_sasl_security_options[[:>:]];<a href="postconf.5.html#smtpd_sasl_security_options">&</a>;g
	s;[[:<:]]smtpd_sender_login_maps[[:>:]];<a href="postconf.5.html#smtpd_sender_login_maps">&</a>;g
	s;[[:<:]]smtpd_sender_restrictions[[:>:]];<a href="postconf.5.html#smtpd_sender_restrictions">&</a>;g
	s;[[:<:]]smtpd_soft_error_limit[[:>:]];<a href="postconf.5.html#smtpd_soft_error_limit">&</a>;g
	s;[[:<:]]smtpd_timeout[[:>:]];<a href="postconf.5.html#smtpd_timeout">&</a>;g
	s;[[:<:]]soft_bounce[[:>:]];<a href="postconf.5.html#soft_bounce">&</a>;g
	s;[[:<:]]stale_lock_time[[:>:]];<a href="postconf.5.html#stale_lock_time">&</a>;g
	s;[[:<:]]strict_7bit_headers[[:>:]];<a href="postconf.5.html#strict_7bit_headers">&</a>;g
	s;[[:<:]]strict_8bitmime[[:>:]];<a href="postconf.5.html#strict_8bitmime">&</a>;g
	s;[[:<:]]strict_8bitmime_body[[:>:]];<a href="postconf.5.html#strict_8bitmime_body">&</a>;g
	s;[[:<:]]strict_mime_encoding_domain[[:>:]];<a href="postconf.5.html#strict_mime_encoding_domain">&</a>;g
	s;[[:<:]]strict_rfc821_envelopes[[:>:]];<a href="postconf.5.html#strict_rfc821_envelopes">&</a>;g
	s;[[:<:]]sun_mailtool_compatibility[[:>:]];<a href="postconf.5.html#sun_mailtool_compatibility">&</a>;g
	s;[[:<:]]swap_bangpath[[:>:]];<a href="postconf.5.html#swap_bangpath">&</a>;g
	s;[[:<:]]syslog_facility[[:>:]];<a href="postconf.5.html#syslog_facility">&</a>;g
	s;[[:<:]]syslog_name[[:>:]];<a href="postconf.5.html#syslog_name">&</a>;g
	s;[[:<:]]trace_service_name[[:>:]];<a href="postconf.5.html#trace_service_name">&</a>;g
	s;[[:<:]]transport_maps[[:>:]];<a href="postconf.5.html#transport_maps">&</a>;g
	s;[[:<:]]transport_retry_time[[:>:]];<a href="postconf.5.html#transport_retry_time">&</a>;g
	s;[[:<:]]trigger_timeout[[:>:]];<a href="postconf.5.html#trigger_timeout">&</a>;g
	s;[[:<:]]undisclosed_recip[-</bB>]*\n* *[<bB>]*ients_header[[:>:]];<a href="postconf.5.html#undisclosed_recipients_header">&</a>;g
	s;[[:<:]]unknown_address_reject_code[[:>:]];<a href="postconf.5.html#unknown_address_reject_code">&</a>;g
	s;[[:<:]]unknown_client_reject_code[[:>:]];<a href="postconf.5.html#unknown_client_reject_code">&</a>;g
	s;[[:<:]]unknown_hostname_reject_code[[:>:]];<a href="postconf.5.html#unknown_hostname_reject_code">&</a>;g
	s;[[:<:]]unknown_local_recip[-</bB>]*\n* *[<bB>]*ient_reject_code[[:>:]];<a href="postconf.5.html#unknown_local_recipient_reject_code">&</a>;g
	s;[[:<:]]unknown_relay_recipi[-</bB>]*\n*[ <bB>]*ent_reject_code[[:>:]];<a href="postconf.5.html#unknown_relay_recipient_reject_code">&</a>;g
	s;[[:<:]]unknown_virtual_alias_reject_code[[:>:]];<a href="postconf.5.html#unknown_virtual_alias_reject_code">&</a>;g
	s;[[:<:]]unknown_virtual_mail[-</bB>]*\n* *[<bB>]*box_reject_code[[:>:]];<a href="postconf.5.html#unknown_virtual_mailbox_reject_code">&</a>;g
	s;[[:<:]]unverified_recip[-</bB>]*\n* *[<bB>]*ient_reject_code[[:>:]];<a href="postconf.5.html#unverified_recipient_reject_code">&</a>;g
	s;[[:<:]]unverified_sender_reject_code[[:>:]];<a href="postconf.5.html#unverified_sender_reject_code">&</a>;g
	s;[[:<:]]verp_delimiter_filter[[:>:]];<a href="postconf.5.html#verp_delimiter_filter">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_alias_domains[[:>:]];<a href="postconf.5.html#virtual_alias_domains">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_alias_expansion_limit[[:>:]];<a href="postconf.5.html#virtual_alias_expansion_limit">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_alias_maps[[:>:]];<a href="postconf.5.html#virtual_alias_maps">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_maps[[:>:]];<a href="postconf.5.html#virtual_maps">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_alias_recursion_limit[[:>:]];<a href="postconf.5.html#virtual_alias_recursion_limit">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_gid_maps[[:>:]];<a href="postconf.5.html#virtual_gid_maps">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_mail[-</bB>]*\n* *[<bB>]*box_base[[:>:]];<a href="postconf.5.html#virtual_mailbox_base">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_mail[-</bB>]*\n* *[<bB>]*box_domains[[:>:]];<a href="postconf.5.html#virtual_mailbox_domains">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_mail[-</bB>]*\n* *[<bB>]*box_limit[[:>:]];<a href="postconf.5.html#virtual_mailbox_limit">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_mail[-</bB>]*\n* *[<bB>]*box_lock[[:>:]];<a href="postconf.5.html#virtual_mailbox_lock">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_mail[-</bB>]*\n* *[<bB>]*box_maps[[:>:]];<a href="postconf.5.html#virtual_mailbox_maps">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_minimum_uid[[:>:]];<a href="postconf.5.html#virtual_minimum_uid">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_transport[[:>:]];<a href="postconf.5.html#virtual_transport">&</a>;g
	s;[[:<:]]vir[-</bB>]*\n*[ <bB>]*tual_uid_maps[[:>:]];<a href="postconf.5.html#virtual_uid_maps">&</a>;g

	# Undo hyperlinks of manual pages with the same name as parameters.

	s/<a href="[^"]*">\([^<]*\)<\/a>(/\1(/g

	# Undo hyperlinks of pathnames thay collide with parameter names.

	s/\/<a href="[^"]*">\([^<]*\)<\/a>/\/\1/g

	# Hyperlink Postfix manual page references.

	s/[<bB>]*anvil[</bB>]*(8)/<a href="anvil.8.html">&<\/a>/g
	s/[<bB>]*bounce[</bB>]*(8)/<a href="bounce.8.html">&<\/a>/g
	s/[<bB>]*cleanup[</bB>]*(8)/<a href="cleanup.8.html">&<\/a>/g
	s/[<bB>]*defer[</bB>]*(8)/<a href="defer.8.html">&<\/a>/g
	s/[<bB>]*error[</bB>]*(8)/<a href="error.8.html">&<\/a>/g
	s/[<bB>]*flush[</bB>]*(8)/<a href="flush.8.html">&<\/a>/g
	s/[<bB>]*lmtp[</bB>]*(8)/<a href="lmtp.8.html">&<\/a>/g
	s/[<bB>]*local[</bB>]*(8)/<a href="local.8.html">&<\/a>/g
	s/[<bB>]*mas[-</bB>]*\n* *[<bB>]*ter[</bB>]*(8)/<a href="master.8.html">&<\/a>/g
	s/[<bB>]*pickup[</bB>]*(8)/<a href="pickup.8.html">&<\/a>/g
	s/[<bB>]*pipe[</bB>]*(8)/<a href="pipe.8.html">&<\/a>/g
	s/[<bB>]*oqmgr[</bB>]*(8)/<a href="qmgr.8.html">&<\/a>/g
	s/[<bB>]*[[:<:]]qmgr[</bB>]*(8)/<a href="qmgr.8.html">&<\/a>/g
	s/[<bB>]*qmqpd[</bB>]*(8)/<a href="qmqpd.8.html">&<\/a>/g
	s/[<bB>]*showq[</bB>]*(8)/<a href="showq.8.html">&<\/a>/g
	s/[<bB>]*smtp[</bB>]*(8)/<a href="smtp.8.html">&<\/a>/g
	s/[<bB>]*smtpd[</bB>]*(8)/<a href="smtpd.8.html">&<\/a>/g
	s/[<bB>]*spawn[</bB>]*(8)/<a href="spawn.8.html">&<\/a>/g
	s/[<bB>]*trace[</bB>]*(8)/<a href="trace.8.html">&<\/a>/g
	s/[<bB>]*trivial- *<br> *rewrite[</bB>]*(8)/<a href="trivial-rewrite.8.html">&<\/a>/g
	s/[<bB>]*triv[-</bB>]*\n* *[<bB>]*ial-[</bB>]*\n* *[<bB>]*rewrite[</bB>]*(8)/<a href="trivial-rewrite.8.html">&<\/a>/g
	s/[<bB>]*mailq[</bB>]*(1)/<a href="mailq.1.html">&<\/a>/g
	s/[<bB>]*newaliases[</bB>]*(1)/<a href="newaliases.1.html">&<\/a>/g
	s/[<bB>]*postalias[</bB>]*(1)/<a href="postalias.1.html">&<\/a>/g
	s/[<bB>]*postcat[</bB>]*(1)/<a href="postcat.1.html">&<\/a>/g
	s/[<bB>]*postconf[</bB>]*(1)/<a href="postconf.1.html">&<\/a>/g
	s/[<bB>]*postdrop[</bB>]*(1)/<a href="postdrop.1.html">&<\/a>/g
	s/[<bB>]*postfix[</bB>]*(1)/<a href="postfix.1.html">&<\/a>/g
	s/[<bB>]*postkick[</bB>]*(1)/<a href="postkick.1.html">&<\/a>/g
	s/[<bB>]*postlock[</bB>]*(1)/<a href="postlock.1.html">&<\/a>/g
	s/[<bB>]*postlog[</bB>]*(1)/<a href="postlog.1.html">&<\/a>/g
	s/[<bB>]*postmap[</bB>]*(1)/<a href="postmap.1.html">&<\/a>/g
	s/[<bB>]*postqueue[</bB>]*(1)/<a href="postqueue.1.html">&<\/a>/g
	s/[<bB>]*postsuper[</bB>]*(1)/<a href="postsuper.1.html">&<\/a>/g
	s/[<bB>]*send[-</bB>]*\n*[ <bB>]*mail[</bB>]*(1)/<a href="sendmail.1.html">&<\/a>/g
	s/[<bB>]*smtp-[</bB>]*\n* *[<bB>]*source[</bB>]*(1)/<a href="smtp-source.1.html">&<\/a>/g
	s/[<bB>]*smtp-[</bB>]*\n* *[<bB>]*sink[</bB>]*(1)/<a href="smtp-sink.1.html">&<\/a>/g
	s/[<bB>]*qmqp-[</bB>]*\n* *[<bB>]*source[</bB>]*(1)/<a href="qmqp-source.1.html">&<\/a>/g
	s/[<bB>]*qmqp-[</bB>]*\n* *[<bB>]*sink[</bB>]*(1)/<a href="qmqp-sink.1.html">&<\/a>/g
	s/[<bB>]*qshape[</bB>]*(1)/<a href="qshape.1.html">&<\/a>/g
	s/[<bB>]*access[</bB>]*(5)/<a href="access.5.html">&<\/a>/g
	s/[<bB>]*aliases[</bB>]*(5)/<a href="aliases.5.html">&<\/a>/g
	s/[<bB>]*canonical[</bB>]*(5)/<a href="canonical.5.html">&<\/a>/g
	s/[<bB>]*etrn[</bB>]*(5)/<a href="etrn.5.html">&<\/a>/g
	s/[<bB>]*ldap[</bBiI>]*_[</iIbB>]*table[</bB>]*(5)/<a href="ldap_table.5.html">&<\/a>/g
	s/[<bB>]*mysql[</bBiI>]*_[</iIbB>]*table[</bB>]*(5)/<a href="mysql_table.5.html">&<\/a>/g
	s/[<bB>]*pcre[</bBiI>]*_[</iIbB>]*table[</bB>]*(5)/<a href="pcre_table.5.html">&<\/a>/g
	s/[<bB>]*pgsql[</bBiI>]*_[</iIbB>]*table[</bB>]*(5)/<a href="pgsql_table.5.html">&<\/a>/g
	s/[<bB>]*postconf[</bB>]*(5)/<a href="postconf.5.html">&<\/a>/g
	s/[<bB>]*proxymap[</bB>]*(8)/<a href="proxymap.8.html">&<\/a>/g
	s/[<bB>]*reg[-</bB>]*\n*[ <bB>]*exp[</bBiI>]*_[</iIbB>]*table[</bB>]*(5)/<a href="regexp_table.5.html">&<\/a>/g
	s/[<bB>]*relocated[</bB>]*(5)/<a href="relocated.5.html">&<\/a>/g
	s/[<bB>]*trans[-</bB>]*\n*[ <bB>]*port[</bB>]*(5)/<a href="transport.5.html">&<\/a>/g
	s/[<bB>]*verify[</bB>]*(8)/<a href="verify.8.html">&<\/a>/g
	s/[<bB>]*virtual[</bB>]*(5)/<a href="virtual.5.html">&<\/a>/g
	s/[<bB>]*virtual[</bB>]*(8)/<a href="virtual.8.html">&<\/a>/g
	s/[<bB>]*cidr_table[</bB>]*(5)/<a href="cidr_table.5.html">&<\/a>/g
	s/[<bB>]*tcp_table[</bB>]*(5)/<a href="tcp_table.5.html">&<\/a>/g
	s/[<bB>]*body_checks[</bB>]*(5)/<a href="header_checks.5.html">&<\/a>/g
	s/[<bB>]*header_checks[</bB>]*(5)/<a href="header_checks.5.html">&<\/a>/g

	# Hyperlink README document names

	s/[[:<:]][A-Z_]*_README[[:>:]]/<a href="&.html">&<\/a>/g
	s/[[:<:]]INSTALL[[:>:]]/<a href="&.html">&<\/a>/g
	s/[[:<:]]OVERVIEW[[:>:]]/<a href="&.html">&<\/a>/g
	s/"type:table"/"<a href="DATABASE_README.html">type:table<\/a>"/g

	# Split manual page hyperlinks across newlines

	s/\(<a href="[^"]*">\)\([<bB>]*[-a-z0-9_]*[-</bB>]*\)\(\n *\)\([<bB>]*[-a-z0-9_]*[</bB>]*([0-9])\)\(<\/a>\)/\1\2\5\3\1\4\5/

	# Access restrictions - generic

	s;[[:<:]]check_policy_service[[:>:]];<a href="postconf.5.html#check_policy_service">&</a>;g
	s;[[:<:]]defer_if_permit[[:>:]];<a href="postconf.5.html#defer_if_permit">&</a>;g
	s;[[:<:]]defer_if_reject[[:>:]];<a href="postconf.5.html#defer_if_reject">&</a>;g
	s;[[:<:]]reject_multi_recip[-</bB>]*\n* *[<bB>]*ient_bounce[[:>:]];<a href="postconf.5.html#reject_multi_recipient_bounce">&</a>;g
	s;[[:<:]]reject_unauth_pipelining[[:>:]];<a href="postconf.5.html#reject_unauth_pipelining">&</a>;g
	s;[[:<:]]warn_if_reject[[:>:]];<a href="postconf.5.html#warn_if_reject">&</a>;g

	# Access restrictions - client

	s;[[:<:]]check_client_access[[:>:]];<a href="postconf.5.html#check_client_access">&</a>;g
	s;[[:<:]]permit_mynetworks[[:>:]];<a href="postconf.5.html#permit_mynetworks">&</a>;g
	s;[[:<:]]reject_unknown_client[[:>:]];<a href="postconf.5.html#reject_unknown_client">&</a>;g
	s;[[:<:]]reject_rbl_client[[:>:]];<a href="postconf.5.html#reject_rbl_client">&</a>;g
	s;[[:<:]]reject_rhsbl_client[[:>:]];<a href="postconf.5.html#reject_rhsbl_client">&</a>;g

	# Access restrictions - helo

	s;[[:<:]]check_helo_access[[:>:]];<a href="postconf.5.html#check_helo_access">&</a>;g
	s;[[:<:]]reject_invalid_hostname[[:>:]];<a href="postconf.5.html#reject_invalid_hostname">&</a>;g
	s;[[:<:]]reject_non_fqdn_hostname[[:>:]];<a href="postconf.5.html#reject_non_fqdn_hostname">&</a>;g
	s;[[:<:]]reject_unknown_hostname[[:>:]];<a href="postconf.5.html#reject_unknown_hostname">&</a>;g

	# Access restrictions - sender

	s;[[:<:]]check_sender_access[[:>:]];<a href="postconf.5.html#check_sender_access">&</a>;g
	s;[[:<:]]\(reject_authenti\)\([-</bB>]*\n*[ <bB>]*\)\(cated_sender_login_mismatch\)[[:>:]];<a href="postconf.5.html#reject_authenticated_sender_login_mismatch">\1<\/a>\2<a href="postconf.5.html#reject_authenticated_sender_login_mismatch">\3</a>;g
	s;[[:<:]]reject_non_fqdn_sender[[:>:]];<a href="postconf.5.html#reject_non_fqdn_sender">&</a>;g
	s;[[:<:]]reject_rhsbl_sender[[:>:]];<a href="postconf.5.html#reject_rhsbl_sender">&</a>;g
	s;[[:<:]]reject_sender_login_mis[-</bB>]*\n*[ <bB>]*match[[:>:]];<a href="postconf.5.html#reject_sender_login_mismatch">&</a>;g
	s;[[:<:]]reject_unauthenticated_sender_login_mismatch[[:>:]];<a href="postconf.5.html#reject_unauthenticated_sender_login_mismatch">&</a>;g
	s;[[:<:]]reject_unknown_sender_domain[[:>:]];<a href="postconf.5.html#reject_unknown_sender_domain">&</a>;g
	s;[[:<:]]reject_unlisted_sender[[:>:]];<a href="postconf.5.html#reject_unlisted_sender">&</a>;g
	s;[[:<:]]reject_unveri[-</bB>]*\n*[ <bB>]*fied_sender[[:>:]];<a href="postconf.5.html#reject_unverified_sender">&</a>;g

	# Access restrictions - recip[-</bB>]*\n* *[<bB>]*ient

	s;[[:<:]]check_recip[-</bB>]*\n* *[<bB>]*ient_access[[:>:]];<a href="postconf.5.html#check_recipient_access">&</a>;g
	s;[[:<:]]check_recip[-</bB>]*\n* *[<bB>]*ient_mx_access[[:>:]];<a href="postconf.5.html#check_recipient_mx_access">&</a>;g
	s;[[:<:]]check_recip[-</bB>]*\n* *[<bB>]*ient_ns_access[[:>:]];<a href="postconf.5.html#check_recipient_ns_access">&</a>;g
	s;[[:<:]]permit_auth_destination[[:>:]];<a href="postconf.5.html#permit_auth_destination">&</a>;g
	s;[[:<:]]permit_mx_backup[[:>:]];<a href="postconf.5.html#permit_mx_backup">&</a>;g
	s;[[:<:]]reject_non_fqdn_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#reject_non_fqdn_recipient">&</a>;g
	s;[[:<:]]reject_rhsbl_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#reject_rhsbl_recipient">&</a>;g
	s;[[:<:]]reject_unauth_destination[[:>:]];<a href="postconf.5.html#reject_unauth_destination">&</a>;g
	s;[[:<:]]reject_unknown_recipi[-</bB>]*\n*[ <bB>]*ent_domain[[:>:]];<a href="postconf.5.html#reject_unknown_recipient_domain">&</a>;g
	s;[[:<:]]reject_unlisted_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#reject_unlisted_recipient">&</a>;g
	s;[[:<:]]reject_unveri[-</bB>]*\n*[ <bB>]*fied_recip[-</bB>]*\n* *[<bB>]*ient[[:>:]];<a href="postconf.5.html#reject_unverified_recipient">&</a>;g

	# Access restrictions - etrn

	s;[[:<:]]check_etrn_access[[:>:]];<a href="postconf.5.html#check_etrn_access">&</a>;g

	# Split parameter or restriction hyperlinks across line breaks

	s/\(<a href="[^"]*">\)\([-a-z0-9_]*\)[[:>:]]\([-</bB>]*\n *[<bB>]*\)[[:<:]]\([-a-z0-9_]*\)\(<\/a>\)/\1\2\5\3\1\4\5/

	# Glue manual/parameter/restriction hyperlinks without line breaks.

	s/\(<a href="[^"]*">\)\([<bB>]*[-a-zA-Z0-9._]*[<bB>]*\)<\/a>\1/\1\2/g
	s/\(<a href="[^"]*">\)\([<bB>]*[-a-zA-Z0-9._]*[<bB>]*\)<\/a>\1/\1\2/g

	# Hyperlink URLs and RFC documents

	s/\(https?:\/\/[^ ,"()]*[^ ,"():;!?.]\)/<a href="\1">\1<\/a>/
	s/\(ftp:\/\/[^ ,"()]*[^ ,"():;!?.]\)/<a href="\1">\1<\/a>/
	s/[[:<:]]RFC *\([1-9][0-9]*\)/<a href="https:\/\/www.faqs.org\/rfcs\/rfc\1.html">&<\/a>/

	# Hyperlink phrases not in headers.

	/<\/*h[0-9]>/{
		p
		d
	}
	s/canonical domains*/<a href="VIRTUAL_README.html#canonical">&<\/a>/
	s/hosted domains*/<a href="VIRTUAL_README.html#canonical">&<\/a>/
	#s/other domains*/<a href="VIRTUAL_README.html#canonical">&<\/a>/
	s/virtual alias example/<a href="VIRTUAL_README.html#virtual_alias">&<\/a>/
	s/virtual mailbox example/<a href="VIRTUAL_README.html#virtual_mailbox">&<\/a>/
	s/local domains*/<a href="ADDRESS_CLASS_README.html#local_domain_class">&<\/a>/
	s/virtual alias domains*/<a href="ADDRESS_CLASS_README.html#virtual_alias_class">&<\/a>/
	s/virtual ALIAS domains*/<a href="ADDRESS_CLASS_README.html#virtual_alias_class">&<\/a>/
	s/virtual mailbox domains*/<a href="ADDRESS_CLASS_README.html#virtual_mailbox_class">&<\/a>/
	s/virtual MAILBOX domains*/<a href="ADDRESS_CLASS_README.html#virtual_mailbox_class">&<\/a>/
	s/relay domains*/<a href="ADDRESS_CLASS_README.html#relay_domain_class">&<\/a>/
	s/default domains*/<a href="ADDRESS_CLASS_README.html#default_domain_class">&<\/a>/
	s/mydestination domains*/<a href="ADDRESS_CLASS_README.html#local_domain_class">&<\/a>/
	s/[[:<:]]"*maildrop"* *queues*[[:>:]]/<a href="QSHAPE_README.html#maildrop_queue">&<\/a>/
	s/[[:<:]]\("*maildrop"*\),/<a href="QSHAPE_README.html#maildrop_queue">\1<\/a>,/
	s/[[:<:]]\("*incoming"*\) and[[:>:]]/<a href="QSHAPE_README.html#incoming_queue">\1<\/a> and/
	s/[[:<:]]\("*incoming"*\) or[[:>:]]/<a href="QSHAPE_README.html#incoming_queue">\1<\/a> or/
	s/[[:<:]]"*incoming"* *queues*[[:>:]]/<a href="QSHAPE_README.html#incoming_queue">&<\/a>/
	s/<b> *incoming *<\/b> *queues*[[:>:]]/<a href="QSHAPE_README.html#incoming_queue">&<\/a>/
	s/[[:<:]]"*active"* *queues*[[:>:]]/<a href="QSHAPE_README.html#active_queue">&<\/a>/
	s/[[:<:]]"*deferred"* *queues*[[:>:]]/<a href="QSHAPE_README.html#deferred_queue">&<\/a>/
	s/[[:<:]]"*hold"* *queues*[[:>:]]/<a href="QSHAPE_README.html#hold_queue">&<\/a>/
	s/[[:<:]]\("*hold"*\),/<a href="QSHAPE_README.html#hold_queue">\1<\/a>,/

	# Hyperlink map types.

	s/[[:<:]]\(cidr\):/<a href="cidr_table.5.html">\1<\/a>:/g
	s/[[:<:]]\(pcre\):/<a href="pcre_table.5.html">\1<\/a>:/g
	s/[[:<:]]\(proxy\):/<a href="proxymap.8.html">\1<\/a>:/g
	s/[[:<:]]\(pgsql\):/<a href="pgsql_table.5.html">\1<\/a>:/g
	s/[[:<:]]\(mysql\):/<a href="mysql_table.5.html">\1<\/a>:/g
	s/[[:<:]]\(ldap\):/<a href="ldap_table.5.html">\1<\/a>:/g
	s/[[:<:]]\(regexp\):/<a href="regexp_table.5.html">\1<\/a>:/g
	s/[[:<:]]\(tcp\):/<a href="tcp_table.5.html">\1<\/a>:/g

	# Do nice links for smtp:host:port etc.

	s/[[:<:]]\(error\):/<a href="error.8.html">\1<\/a>:/g
	s/[[:<:]]\(smtp\):/<a href="smtp.8.html">\1<\/a>:/g
	s/[[:<:]]\(lmtp\):/<a href="lmtp.8.html">\1<\/a>:/g

' "$@"
