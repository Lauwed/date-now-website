<script lang="ts">
	import { onMount } from 'svelte';
	import Card from './Card.svelte';
	import Logo from './Logo.svelte';
	import Nav from './Nav.svelte';
	import Socials from './Socials.svelte';
	import TwitchButton from './TwitchButton.svelte';

	let { currentPath } = $props();

	let isLive = $state(false);

	onMount(async () => {
		try {
			// const response = await fetch('/api/twitch-status');
			// const result = await response.json();
			// isLive = result.isLive;
		} catch (error) {
			console.error('Error fetching Twitch status:', error);
		}
	});

	const pages = [
		{ name: 'Home', path: '/' },
		{
			name: 'About',
			path: '/about'
		},
		{
			name: 'Archive',
			path: '/archive'
		}
	];
</script>

<Card tag="header" customClass="header" --blur="none">
	<a class="header__logo" href="/"><Logo variant="symbol" size="sm" /></a>

	<!-- ON STREAM NOW -->
	<TwitchButton {isLive} />

	<Nav {currentPath} {pages} />

	<Socials />
</Card>

<style lang="scss">
	@use './../styles/variables' as *;
	@use './../styles/mixins.scss' as *;

	:global(.header) {
		display: flex;
		align-items: center;
		justify-content: space-between;
		position: relative;

		@include sm {
			flex-direction: column;
			gap: 16px;
		}
	}

	.header {
		&__logo {
			display: flex;
			justify-content: center;
			align-items: center;
			border-bottom: 0;
		}
	}
</style>
